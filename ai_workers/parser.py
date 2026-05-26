#!/usr/bin/env python3
"""
SARA AI Parser v2.0 — Planner + Orchestrator
Outputs: { success, intent, execution_plan: [...], response_text, needs_clarification }
"""
import argparse
import json
import sys
from parser_router import parse_command

def load_input(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def write_output(path: str, data: dict):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)

def main():
    parser = argparse.ArgumentParser(description="SARA AI Planner v2.0")
    parser.add_argument("--input",  required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    try:
        data   = load_input(args.input)
        result = parse_command(data)
        if not isinstance(result, dict):
            result = {"success": False, "error": "Invalid result format"}
        write_output(args.output, result)
        sys.exit(0 if result.get("success") else 1)
    except Exception as e:
        write_output(args.output, {"success": False, "error": str(e)})
        sys.exit(1)

if __name__ == "__main__":
    main()
