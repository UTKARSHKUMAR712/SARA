"""
SARA Context Manager v2 — DAG-aware context compression.
Keeps context under 600 tokens. Knows about TaskGraph node states.
"""
import json
import os

PLANS_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "data", "plans")


def save_plan(session_id: str, plan: dict):
    os.makedirs(PLANS_DIR, exist_ok=True)
    try:
        with open(os.path.join(PLANS_DIR, f"{session_id}.json"), "w", encoding="utf-8") as f:
            json.dump(plan, f, indent=2)
    except Exception:
        pass


def load_plan(session_id: str) -> dict | None:
    path = os.path.join(PLANS_DIR, f"{session_id}.json")
    if os.path.exists(path):
        try:
            with open(path, encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return None


def clear_plan(session_id: str):
    path = os.path.join(PLANS_DIR, f"{session_id}.json")
    try:
        if os.path.exists(path):
            os.remove(path)
    except Exception:
        pass


def extract_tool_results(history: list, n: int = 3) -> list:
    """Get last N tool result system messages from history."""
    results = []
    for msg in reversed(history):
        content = msg.get("content", "")
        if msg.get("role") == "system" and "Tool result:" in content:
            results.append(content)
            if len(results) >= n:
                break
    return list(reversed(results))


def extract_user_goal(history: list) -> str:
    """Get the original user goal — first user message, stripped of system suffixes."""
    for msg in history:
        if msg.get("role") == "user":
            content = msg.get("content", "")
            content = content.split("\n\n[SYSTEM MESSAGE]")[0].strip()
            return content
    return ""


def build_executor_context(history: list, plan: dict | None, memory_context: str = "") -> str:
    """
    Build compact context for executor AI call.
    Target: ~500 tokens.
    """
    parts = []

    if memory_context:
        parts.append(f"USER CONTEXT:\n{memory_context[:200]}")

    goal = extract_user_goal(history)
    if goal:
        parts.append(f"GOAL: {goal}")

    if plan:
        nodes = plan.get("nodes", [])
        done = [f"{n['id']}.{n['desc']}" for n in nodes if n.get("status") == "done"]
        running = [f"{n['id']}.{n['desc']}" for n in nodes if n.get("status") == "pending" and
                   all(d in {x["id"] for x in nodes if x.get("status") == "done"}
                       for d in n.get("depends_on", []))]
        if done:
            parts.append(f"DONE: {', '.join(done[:4])}")
        if running:
            parts.append(f"NEXT: {running[0]}")

    results = extract_tool_results(history, n=2)
    if results:
        parts.append("LAST RESULTS:")
        for r in results:
            parts.append(r[:350] + ("..." if len(r) > 350 else ""))

    return "\n\n".join(parts)
