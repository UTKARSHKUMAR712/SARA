"""
SARA Cognitive Orchestrator v3 — Fixed double-parsing bug, settings.json config loading.

CRITICAL FIX: call_ai_api() returns a parsed dict {success, execution_plan, response_text}.
              The old orchestrator tried to re-parse it as raw choices → always empty steps.
              Now we use the parsed result directly.

Per-invocation flow:
  1. Load config from settings.json (active profile) if C++ didn't pass a valid one
  2. Tier Router (0 AI tokens for simple tasks)
  3. Planner: goal → DAG TaskGraph (once per new goal, ~200 tokens)
  4. Verifier + Reflector on loop continuations
  5. Executor: picks next DAG-ready node, targeted tool selection
"""
import json
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from parser_api import call_ai_api
from parser_tools import build_tools, get_tools_for_category
from cognition.tier_router import classify_tier
from cognition.planner import (
    create_plan, get_ready_node, mark_node_done, mark_node_failed,
    retry_node, is_plan_complete, is_plan_blocked, plan_summary
)
from cognition.verifier import verify_step
from cognition.reflector import reflect_and_fix
from cognition.executor import select_tools_for_subtask, build_executor_prompt, EXECUTOR_SYSTEM
from cognition.context_manager import (
    save_plan, load_plan, clear_plan,
    extract_tool_results, extract_user_goal, build_executor_context,
)

# ─────────────────────────────────────────────────────────────────────────────
# Settings.json config loader — reads directly from disk so Python is independent
# of what C++ decides to pass
# ─────────────────────────────────────────────────────────────────────────────
_SETTINGS_PATH = os.path.join(os.path.dirname(__file__), "..", "..", "settings.json")
_CACHED_CFG: dict = {}

def _load_settings_config() -> dict:
    """Load the active AI provider config from settings.json."""
    global _CACHED_CFG
    if _CACHED_CFG:
        return _CACHED_CFG
    try:
        with open(_SETTINGS_PATH, encoding="utf-8") as f:
            s = json.load(f)
        # Try active_profile first
        profile = s.get("active_profile", "")
        if profile and "ai_profiles" in s and profile in s["ai_profiles"]:
            cfg = dict(s["ai_profiles"][profile])
            cfg.setdefault("max_tokens", 4096)
            cfg.setdefault("temperature", 0.7)
            cfg.setdefault("timeout_seconds", 60)
            cfg.setdefault("thinking_enabled", False)
            _CACHED_CFG = cfg
            return cfg
        # Fallback: ai_primary
        if "ai_primary" in s:
            cfg = dict(s["ai_primary"])
            cfg.setdefault("max_tokens", 4096)
            cfg.setdefault("temperature", 0.7)
            cfg.setdefault("timeout_seconds", 60)
            cfg.setdefault("thinking_enabled", False)
            _CACHED_CFG = cfg
            return cfg
    except Exception:
        pass
    return {}


def _resolve_config(data_cfg: dict) -> dict:
    """
    Merge config from C++ with settings.json.
    settings.json wins if C++ passes empty/unknown provider.
    """
    if data_cfg.get("api_key") and data_cfg.get("endpoint"):
        # C++ passed a valid config — use it, but fill missing keys
        cfg = dict(data_cfg)
        cfg.setdefault("max_tokens", 4096)
        cfg.setdefault("temperature", 0.7)
        cfg.setdefault("timeout_seconds", 60)
        cfg.setdefault("thinking_enabled", False)
        return cfg
    # Fall back to settings.json
    return _load_settings_config()


# ─────────────────────────────────────────────────────────────────────────────
# Dual model config — fast model for cognitive calls, smart for execution
# ─────────────────────────────────────────────────────────────────────────────
def _get_fast_config(data: dict, smart_cfg: dict) -> dict:
    """Fast model: used for tier routing, verification, reflection."""
    fast_raw = data.get("fast_config", {})
    if fast_raw and fast_raw.get("provider") and fast_raw.get("api_key"):
        return _resolve_config(fast_raw)
    # Derive from smart: same endpoint, smaller token budget
    fast = dict(smart_cfg)
    fast["max_tokens"] = min(smart_cfg.get("max_tokens", 4096), 512)
    fast["thinking_enabled"] = False
    return fast


# ─────────────────────────────────────────────────────────────────────────────
# FIXED: call_ai_raw — uses call_ai_api's PARSED result (not raw choices)
# call_ai_api returns {success, execution_plan, response_text, ...}
# NOT {choices: [{message: {content, tool_calls}}]}
# ─────────────────────────────────────────────────────────────────────────────
def call_ai_raw(config: dict, system: str, user: str, tools: list = None) -> dict:
    """
    Lightweight AI call for cognitive sub-tasks (planner, verifier, reflector).
    Returns {success, response_text}.
    """
    messages = [
        {"role": "system", "content": system},
        {"role": "user",   "content": user},
    ]
    result = call_ai_api(config, messages, tools or [])
    if result.get("success") is False:
        return {"success": False, "error": result.get("error", "AI call failed")}
    # call_ai_api returns parsed dict with response_text
    return {
        "success":       True,
        "response_text": result.get("response_text", ""),
    }


# ─────────────────────────────────────────────────────────────────────────────
# Intent classification (for logging)
# ─────────────────────────────────────────────────────────────────────────────
def _classify_intent(steps: list) -> str:
    if not steps:
        return "conversational"
    a = steps[0].get("action", "")
    if a in {"spotify_play", "play_youtube", "media_play_pause", "spotify_pause",
             "media_command"}:
        return "media_playback"
    if a in {"search_google", "open_website"}:
        return "browser_automation"
    return "system_control"


def _make_result(steps, response_text="", needs_continue=None):
    if needs_continue is None:
        needs_continue = (
            bool(steps) and
            not any(s.get("action") in ("task_complete",) for s in steps)
        )
    return {
        "success":                True,
        "intent":                 _classify_intent(steps),
        "execution_plan":         steps,
        "response_text":          response_text,
        "needs_clarification":    False,
        "clarification_question": "",
        "needs_continue":         needs_continue,
        "raw_choice":             {},
    }


# ─────────────────────────────────────────────────────────────────────────────
# Main entry point
# ─────────────────────────────────────────────────────────────────────────────
def parse_command(data: dict) -> dict:
    user_message    = data.get("user_message", "")
    session_context = data.get("session_context", {})
    memory_context  = data.get("memory_context", "")
    dynamic_tools   = data.get("tools", [])

    session_id = (session_context or {}).get("session_id", "default")
    history    = (session_context or {}).get("history", [])

    # Resolve configs (settings.json wins over empty C++ config)
    smart_cfg = _resolve_config(data.get("config", {}))
    fast_cfg  = _get_fast_config(data, smart_cfg)

    # Validate we have a working config
    if not smart_cfg.get("api_key"):
        return _make_result([], "⚠️ No AI API key found. Check settings.json → ai_primary.api_key", False)

    # Is this a loop continuation (tool result feeding back in)?
    is_continuation = (
        "[SYSTEM MESSAGE]" in user_message or
        any(
            m.get("role") == "system" and "Tool result:" in m.get("content", "")
            for m in history[-3:]
        )
    )

    # Original goal (stripped of loop suffixes)
    original_goal = extract_user_goal(history) if is_continuation else user_message
    original_goal = original_goal.split("\n\n[SYSTEM MESSAGE]")[0].strip()

    # ── TIER CLASSIFICATION (0 AI tokens) ─────────────────────────────────────
    tier, category_hint = classify_tier(user_message, history)

    # ── TIER 1: Chat — small AI call for response only ────────────────────────
    if tier == 1 and not is_continuation:
        chat_cfg = dict(fast_cfg)
        chat_cfg["max_tokens"] = 256
        messages = [
            {"role": "system", "content": "You are SARA, a helpful AI assistant. Reply concisely."},
            {"role": "user",   "content": user_message},
        ]
        result = call_ai_api(chat_cfg, messages, [])
        if result.get("success") is False:
            return _make_result([], f"❌ AI error: {result.get('error', 'Unknown')}", False)
        text = result.get("response_text", "").strip() or "👋 Hello!"
        return _make_result([], text, needs_continue=False)

    # ── TOOL REGISTRY ─────────────────────────────────────────────────────────
    all_tools = build_tools()

    # ── DAG PLAN MANAGEMENT ───────────────────────────────────────────────────
    plan = load_plan(session_id)

    if not plan and tier >= 3:
        # Create TaskGraph with smart model
        plan = create_plan(original_goal, smart_cfg, call_ai_raw)
        if plan:
            save_plan(session_id, plan)

    # ── VERIFICATION of last tool result ──────────────────────────────────────
    if plan and is_continuation:
        tool_results = extract_tool_results(history, n=1)
        last_result  = tool_results[-1] if tool_results else ""
        just_ran     = get_ready_node(plan)

        if just_ran and last_result:
            verify = verify_step(just_ran, last_result, fast_cfg, call_ai_raw)

            if verify["status"] == "success":
                plan = mark_node_done(plan, just_ran["id"], last_result[:200])
                save_plan(session_id, plan)

            elif verify["status"] == "failed":
                retry_count = just_ran.get("retries", 0)
                if retry_count < 2:
                    context_sum = build_executor_context(history, plan, memory_context)
                    fix = reflect_and_fix(
                        just_ran,
                        verify["reason"],
                        verify.get("failure_type", "UNKNOWN"),
                        context_sum,
                        fast_cfg,
                        call_ai_raw
                    )
                    plan = mark_node_failed(
                        plan, just_ran["id"],
                        verify["reason"],
                        verify.get("failure_type", "UNKNOWN")
                    )
                    plan = retry_node(plan, just_ran["id"])
                    if fix:
                        for n in plan.get("nodes", []):
                            if n["id"] == just_ran["id"]:
                                n["_override"] = fix
                    save_plan(session_id, plan)
                else:
                    # Max retries — skip
                    for n in plan.get("nodes", []):
                        if n["id"] == just_ran["id"]:
                            n["status"] = "skipped"
                    save_plan(session_id, plan)

    # ── PLAN COMPLETE CHECK ────────────────────────────────────────────────────
    if plan and is_plan_complete(plan):
        clear_plan(session_id)
        done = [{"action": "task_complete", "target": "", "parameters": {},
                 "description": "All tasks done", "delay_seconds": 0}]
        return _make_result(done, "✅ All tasks completed!", needs_continue=False)

    if plan and is_plan_blocked(plan):
        clear_plan(session_id)
        return _make_result([], "⚠️ Task blocked — some steps could not be completed.",
                            needs_continue=False)

    # ── EXECUTOR ──────────────────────────────────────────────────────────────
    current_node = get_ready_node(plan) if plan else None

    if current_node:
        final_tools = select_tools_for_subtask(current_node, all_tools)
    else:
        # Tier 2 single action: focused category subset, max 6 tools
        category_tools = get_tools_for_category(category_hint, all_tools, dynamic_tools)
        final_tools = category_tools[:6] if len(category_tools) > 6 else category_tools

    # Build prompt
    context      = build_executor_context(history, plan, memory_context)
    executor_msg = build_executor_prompt(context, current_node)

    messages = [{"role": "system", "content": EXECUTOR_SYSTEM}]
    for msg in history[-10:]:
        role    = msg.get("role", "user")
        content = msg.get("content", "")
        if role in ("user", "assistant", "system") and content:
            messages.append({"role": role, "content": content[:600]})
    messages.append({"role": "user", "content": executor_msg})

    # ── CALL SMART MODEL (FIXED: use parsed result directly) ──────────────────
    result = call_ai_api(smart_cfg, messages, final_tools)

    if result.get("success") is False:
        err = result.get("error", "Unknown AI error")
        # Show error visibly in Telegram
        return _make_result([], f"❌ AI error: {err}", needs_continue=False)

    # FIXED: call_ai_api returns parsed {execution_plan, response_text} — use directly
    steps         = result.get("execution_plan", [])
    response_text = result.get("response_text", "")

    # Remove any [DEBUG] suffix call_ai_api might have added
    response_text = response_text.split("\n\n[DEBUG]")[0].strip()

    # Add delay_seconds from plan node to each step
    if current_node:
        delay = current_node.get("delay_seconds", 0)
        for step in steps:
            step.setdefault("delay_seconds", delay)

    # Show plan on first iteration (before any tools run)
    if plan and not is_continuation and steps:
        response_text = plan_summary(plan) + ("\n\n" + response_text if response_text else "")

    # Debug line
    response_text = (response_text or "").rstrip()
    response_text += f"\n\n[DEBUG] Tier:{tier} Tools:{len(final_tools)} Steps:{len(steps)}"

    return _make_result(steps, response_text)
