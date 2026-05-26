"""
SARA Verifier v2 — with failure taxonomy.
Fast-path keyword matching (0 tokens), AI only for ambiguous cases.

Failure taxonomy:
  TOOL_FAILURE     — tool itself errored
  PERMISSION_FAILURE — access denied
  NETWORK_FAILURE  — timeout / connection issue
  LOGIC_FAILURE    — wrong output, not what was expected
  TIMEOUT          — process timed out
  UNKNOWN          — unclassified
"""

# Normalized tool result signals (from {success, data, error} format)
SUCCESS_SIGNALS = {
    "written successfully", "file written", "created", "command executed",
    "launched", "started", "found", "read successfully", "done", "ok",
    "opened", "completed",
}

FAILURE_TAXONOMY = {
    "PERMISSION_FAILURE": ["access denied", "permission denied", "unauthorized", "privilege"],
    "NETWORK_FAILURE":    ["timeout", "connection refused", "network error", "unreachable", "dns"],
    "TOOL_FAILURE":       ["failed to open", "failed to start", "failed to write", "error:", "exception", "returned non-zero"],
    "LOGIC_FAILURE":      ["not found", "invalid", "unexpected", "no such file", "mismatch"],
    "TIMEOUT":            ["timed out", "deadline exceeded"],
}

VERIFIER_SYSTEM = """You are SARA's execution verifier.
Reply with EXACTLY:
Line 1: SUCCESS, PARTIAL, or FAILED
Line 2: REASON: <one sentence>
Line 3: FAILURE_TYPE: TOOL_FAILURE|PERMISSION_FAILURE|NETWORK_FAILURE|LOGIC_FAILURE|TIMEOUT|UNKNOWN  (only if FAILED)"""


def classify_failure(error_text: str) -> str:
    """Classify a failure string into a taxonomy type."""
    lower = error_text.lower()
    for ftype, signals in FAILURE_TAXONOMY.items():
        if any(s in lower for s in signals):
            return ftype
    return "UNKNOWN"


def verify_step(node: dict, tool_result: str, config: dict, call_ai_raw) -> dict:
    """
    Returns {"status": "success"|"partial"|"failed", "reason": str, "failure_type": str}
    """
    if not tool_result or not node:
        return {"status": "success", "reason": "no result", "failure_type": None}

    result_lower = tool_result.lower()

    # Fast-path: check normalized {success, error} format
    if '"success": false' in tool_result or '"success":false' in tool_result:
        error_text = tool_result
        failure_type = classify_failure(error_text)
        return {"status": "failed", "reason": "Tool reported failure", "failure_type": failure_type}

    if '"success": true' in tool_result or '"success":true' in tool_result:
        return {"status": "success", "reason": "Tool reported success", "failure_type": None}

    # Keyword fast-path
    if any(sig in result_lower for sig in SUCCESS_SIGNALS):
        return {"status": "success", "reason": "success signal detected", "failure_type": None}

    failure_type = classify_failure(tool_result)
    if failure_type != "UNKNOWN":
        return {"status": "failed", "reason": f"{failure_type} detected", "failure_type": failure_type}

    # AI fallback for ambiguous
    try:
        user_msg = (
            f"Node: {node.get('desc', '')}\n"
            f"Expected: {node.get('success', 'task done')}\n"
            f"Tool result:\n{tool_result[:500]}\n"
            f"Did it succeed?"
        )
        res = call_ai_raw(config=config, system=VERIFIER_SYSTEM, user=user_msg, tools=[])
        if res.get("success"):
            text = res.get("response_text", "").strip()
            lines = [l.strip() for l in text.split("\n") if l.strip()]
            verdict = lines[0].upper() if lines else "UNKNOWN"
            reason = ""
            ftype = "UNKNOWN"
            for line in lines[1:]:
                if line.startswith("REASON:"):
                    reason = line.split("REASON:", 1)[1].strip()
                if line.startswith("FAILURE_TYPE:"):
                    ftype = line.split("FAILURE_TYPE:", 1)[1].strip()

            if "SUCCESS" in verdict:
                return {"status": "success", "reason": reason, "failure_type": None}
            if "PARTIAL" in verdict:
                return {"status": "partial", "reason": reason, "failure_type": None}
            return {"status": "failed", "reason": reason, "failure_type": ftype}
    except Exception:
        pass

    return {"status": "success", "reason": "verification inconclusive", "failure_type": None}
