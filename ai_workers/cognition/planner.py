"""
SARA Planner v2 — DAG-based TaskGraph decomposition.
Generates TaskNode objects with dependency tracking.
ONE AI call per goal (~200 tokens). Called once per new goal.
"""
import json
import re
import time

PLANNER_SYSTEM = """You are SARA's task planner. Decompose the user's goal into a TaskGraph.

Output ONLY valid JSON — no markdown, no text:
{
  "goal": "<one sentence summary>",
  "nodes": [
    {
      "id": 1,
      "desc": "<what to do>",
      "tool_hint": "<best_tool>",
      "success": "<expected output>",
      "status": "pending",
      "retries": 0,
      "depends_on": [],
      "delay_seconds": 0
    }
  ]
}

tool_hint values: write_file, read_file, list_dir, open_app, run_powershell, run_cmd, media_command, take_screenshot, notify, task_complete

Rules:
- Each node maps to exactly ONE tool call
- write_file auto-creates parent folders — no mkdir needed
- Use depends_on to express ordering (node IDs that must complete first)
- If the user says "after X seconds" or "wait X seconds", set delay_seconds on that node
- Max 8 nodes
- Last node should be task_complete with depends_on all prior nodes
- Keep descriptions short and action-oriented
"""


def create_plan(goal: str, config: dict, call_ai_raw) -> dict | None:
    """
    Ask AI to decompose goal into a TaskGraph.
    Returns plan dict or None on failure.
    """
    try:
        res = call_ai_raw(
            config=config,
            system=PLANNER_SYSTEM,
            user=f"Goal: {goal}",
            tools=[]
        )
        if not res.get("success"):
            return None

        text = res.get("response_text", "").strip()
        text = re.sub(r'```(?:json)?\s*|\s*```', '', text).strip()
        match = re.search(r'\{.*\}', text, re.DOTALL)
        if match:
            text = match.group(0)

        plan = json.loads(text)
        if "nodes" in plan and isinstance(plan["nodes"], list):
            # Normalize node structure
            for i, node in enumerate(plan["nodes"]):
                node.setdefault("id", i + 1)
                node.setdefault("status", "pending")
                node.setdefault("retries", 0)
                node.setdefault("depends_on", [])
                node.setdefault("result", None)
                node.setdefault("error", None)
                node.setdefault("failure_type", None)
            plan["created_at"] = int(time.time())
            plan["version"] = 2
            return plan
    except Exception:
        pass
    return None


def get_ready_node(plan: dict) -> dict | None:
    """
    Get the next node ready to execute:
    - status == 'pending'
    - all depends_on nodes are 'done'
    """
    nodes = plan.get("nodes", [])
    done_ids = {n["id"] for n in nodes if n.get("status") == "done"}

    for node in nodes:
        if node.get("status") != "pending":
            continue
        deps = set(node.get("depends_on", []))
        if deps.issubset(done_ids):
            return node
    return None


def mark_node_done(plan: dict, node_id: int, result: str) -> dict:
    """Mark a node as done with its result."""
    for node in plan.get("nodes", []):
        if node["id"] == node_id:
            node["status"] = "done"
            node["result"] = result
            break
    return plan


def mark_node_failed(plan: dict, node_id: int, error: str, failure_type: str = "UNKNOWN") -> dict:
    """Mark a node as failed."""
    for node in plan.get("nodes", []):
        if node["id"] == node_id:
            node["status"] = "failed"
            node["error"] = error
            node["failure_type"] = failure_type
            node["retries"] = node.get("retries", 0) + 1
            break
    return plan


def retry_node(plan: dict, node_id: int) -> dict:
    """Reset a node to pending for retry."""
    for node in plan.get("nodes", []):
        if node["id"] == node_id:
            node["status"] = "pending"
            node["error"] = None
            break
    return plan


def is_plan_complete(plan: dict) -> bool:
    """True when all non-failed nodes are done."""
    nodes = plan.get("nodes", [])
    if not nodes:
        return True
    return all(n.get("status") in ("done", "skipped") for n in nodes)


def is_plan_blocked(plan: dict) -> bool:
    """True when there are no ready nodes and not complete."""
    return get_ready_node(plan) is None and not is_plan_complete(plan)


def plan_summary(plan: dict) -> str:
    """Generate a human-readable plan summary for Telegram."""
    nodes = plan.get("nodes", [])
    goal = plan.get("goal", "Unknown goal")
    lines = [f"📋 *Plan:* {goal}", ""]
    for node in nodes:
        status = node.get("status", "pending")
        icon = {"pending": "⏳", "running": "🔄", "done": "✅", "failed": "❌", "skipped": "⏭️"}.get(status, "❓")
        lines.append(f"{icon} {node['id']}. {node['desc']}")
    return "\n".join(lines)
