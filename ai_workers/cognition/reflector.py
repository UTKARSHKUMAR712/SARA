"""
SARA Reflector v2 — failure-taxonomy-aware self-correction.
Generates alternative tool calls tailored to the failure type.
Max 2 retries per node. Token cost: ~150 tokens.
"""
import json
import re

REFLECTOR_SYSTEM = """You are SARA's self-correction engine.
A task node failed. Propose ONE alternative tool call to fix it.

Output ONLY valid JSON:
{
  "action": "<tool_name>",
  "parameters": {<key: value>},
  "explanation": "<brief reason>"
}

Tool parameters:
- write_file: {"path": str, "content": str}  (path relative to Desktop)
- read_file: {"path": str}
- list_dir: {"path": str}
- run_powershell: {"command": str}
- run_cmd: {"command": str}
- open_app: {"target": str}
- notify: {"title": str, "message": str}

Failure type guidance:
- PERMISSION_FAILURE: try run_powershell with elevated approach
- NETWORK_FAILURE: retry same action or use alternative
- TOOL_FAILURE: try alternative tool (e.g. run_cmd instead of write_file)
- LOGIC_FAILURE: fix the logic/path/parameters
- TIMEOUT: simplify the command
"""


def reflect_and_fix(
    node: dict,
    failure_reason: str,
    failure_type: str,
    context_summary: str,
    config: dict,
    call_ai_raw
) -> dict | None:
    """
    Returns corrected action dict or None.
    """
    try:
        user_msg = (
            f"Failed node: {node.get('desc', '')}\n"
            f"Failure type: {failure_type}\n"
            f"Failure reason: {failure_reason}\n"
            f"Context:\n{context_summary[:400]}\n\n"
            f"Propose an alternative single tool call."
        )
        res = call_ai_raw(
            config=config,
            system=REFLECTOR_SYSTEM,
            user=user_msg,
            tools=[]
        )
        if not res.get("success"):
            return None

        text = res.get("response_text", "").strip()
        text = re.sub(r'```(?:json)?\s*|\s*```', '', text).strip()
        match = re.search(r'\{.*\}', text, re.DOTALL)
        if match:
            text = match.group(0)

        fix = json.loads(text)
        if "action" in fix and "parameters" in fix:
            return fix
    except Exception:
        pass
    return None
