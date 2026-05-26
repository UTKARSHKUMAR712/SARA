"""
SARA Executor — picks the next tool call given the current subtask.
Uses targeted tool selection (5-8 tools max) instead of all 18.
Token cost: ~300 tokens per call.
"""

EXECUTOR_SYSTEM = """You are SARA, an autonomous OS agent with FULL unrestricted PC access.

RULES (follow exactly):
- Call EXACTLY ONE tool. No explanation text needed.
- write_file(path, content): path relative to Desktop (e.g. test/bakery.html). Auto-creates folders.
- read_file(path): path relative to Desktop.
- list_dir(path): path relative to Desktop.
- run_cmd / run_powershell: working directory = Desktop.
- NEVER repeat a step that already succeeded (check COMPLETED STEPS).
- When ALL goals are done, call task_complete immediately.
"""

# Maps tool_hint to the minimal set of tools needed
TOOL_HINT_MAP: dict = {
    "write_file":     ["write_file", "task_complete"],
    "read_file":      ["read_file", "task_complete"],
    "list_dir":       ["list_dir", "task_complete"],
    "open_app":       ["open_app", "open_website", "task_complete"],
    "run_powershell": ["run_powershell", "run_cmd", "task_complete"],
    "run_cmd":        ["run_cmd", "run_powershell", "task_complete"],
    "media_command":  ["media_command", "task_complete"],
    "notify":         ["notify", "task_complete"],
    "take_screenshot":["take_screenshot", "task_complete"],
    "task_complete":  ["task_complete"],
}

# Fallback tool set for unplanned / Tier 2 tasks
FALLBACK_TOOLS = [
    "write_file", "read_file", "list_dir",
    "open_app", "open_website", "run_powershell",
    "media_command", "take_screenshot", "notify", "task_complete",
]


def select_tools_for_subtask(subtask: dict | None, all_tools: list) -> list:
    """
    Select only the tools relevant to the current subtask.
    Dramatically reduces token usage vs loading all 18+ tools.
    """
    tool_map = {t["function"]["name"]: t for t in all_tools if t.get("type") == "function"}

    hint = subtask.get("tool_hint", "") if subtask else ""
    names = TOOL_HINT_MAP.get(hint, None)
    if not names:
        names = FALLBACK_TOOLS

    return [tool_map[n] for n in names if n in tool_map]


def build_executor_prompt(context: str, subtask: dict | None) -> str:
    """Build the focused user message for the executor AI call."""
    if subtask:
        override = subtask.get("_override")
        if override:
            # Reflector provided a direct fix
            action = override["action"]
            params = override["parameters"]
            return (
                f"{context}\n\n"
                f"RETRY THIS STEP: {subtask.get('desc', '')}\n"
                f"Use {action} with params: {params}"
            )
        return f"{context}\n\nNEXT ACTION: {subtask.get('desc', 'proceed to next step')}"
    return context
