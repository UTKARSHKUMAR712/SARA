"""
SARA parser_router.py — thin shim to cognitive orchestrator.
All logic lives in cognition/orchestrator.py.
"""
from cognition.orchestrator import parse_command  # noqa: F401

__all__ = ["parse_command"]
