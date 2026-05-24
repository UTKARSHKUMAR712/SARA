#!/usr/bin/env python3
"""
SARA AI Parser v2.0 — Planner + Orchestrator
Complete rewrite. No longer a flat tool-caller.
Outputs: { success, intent, execution_plan: [...], response_text, needs_clarification }
"""
import argparse
import json
import os
import sys
import urllib.request
import urllib.error


# ─────────────────────────────────────────────────────────────────────────────
# I/O helpers
# ─────────────────────────────────────────────────────────────────────────────

def load_input(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def write_output(path: str, data: dict):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


# ─────────────────────────────────────────────────────────────────────────────
# Semantic Tool Definitions (what the AI sees — NO raw send_keys / run_cmd)
# ─────────────────────────────────────────────────────────────────────────────

def _delay_param():
    return {
        "delay_seconds": {
            "type": "integer",
            "description": "Pause this many seconds BEFORE executing this step. Use when user says 'after X minutes'."
        }
    }


def build_tools():
    return [
        # ── Browser / Media ─────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "play_youtube",
                "description": "Search and play a video/song on YouTube in the default browser.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "query": {"type": "string", "description": "Search query, e.g. 'Jungkook Seven'"},
                        **_delay_param()
                    },
                    "required": ["query"]
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "media_play_pause",
                "description": "Play or pause the currently playing media/music.",
                "parameters": {"type": "object", "properties": {"action": {"type": "string", "enum": ["play_pause"]}, **_delay_param()}, "required": ["action"]}
            }
        },
        {
            "type": "function",
            "function": {
                "name": "media_next",
                "description": "Skip to the next media/music track.",
                "parameters": {"type": "object", "properties": {"action": {"type": "string", "enum": ["next"]}, **_delay_param()}, "required": ["action"]}
            }
        },
        {
            "type": "function",
            "function": {
                "name": "media_prev",
                "description": "Go back to the previous media/music track.",
                "parameters": {"type": "object", "properties": {"action": {"type": "string", "enum": ["prev"]}, **_delay_param()}, "required": ["action"]}
            }
        },
        {
            "type": "function",
            "function": {
                "name": "media_stop",
                "description": "Stop the currently playing media/music.",
                "parameters": {"type": "object", "properties": {"action": {"type": "string", "enum": ["stop"]}, **_delay_param()}, "required": ["action"]}
            }
        },
        {
            "type": "function",
            "function": {
                "name": "search_google",
                "description": "Search something on Google in the default browser.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "query": {"type": "string", "description": "Search query"},
                        **_delay_param()
                    },
                    "required": ["query"]
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "open_website",
                "description": "Open a specific URL in the default browser.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "url": {"type": "string", "description": "Full URL including https://"},
                        **_delay_param()
                    },
                    "required": ["url"]
                }
            }
        },
        # ── Apps & Processes ─────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "open_app",
                "description": "Open/launch an application by name or path.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "target": {"type": "string", "description": "App name e.g. notepad, chrome, calc, spotify"},
                        **_delay_param()
                    },
                    "required": ["target"]
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "close_process",
                "description": "Close/terminate a running process by its executable name.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "target": {"type": "string", "description": "Process name e.g. notepad.exe, discord.exe"},
                        **_delay_param()
                    },
                    "required": ["target"]
                }
            }
        },
        # ── Surveillance ─────────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "take_screenshot",
                "description": "Capture a screenshot of the full screen and send it back.",
                "parameters": {
                    "type": "object",
                    "properties": {**_delay_param()}
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "capture_camera",
                "description": "Take a photo from the webcam/camera and send it back.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "device_index": {"type": "integer", "description": "Camera index (0 = default)"},
                        **_delay_param()
                    }
                }
            }
        },
        # ── System Control ───────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "run_cmd",
                "description": "Execute a raw shell command using cmd.exe. Useful for creating folders, running scripts, etc.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "command": {"type": "string", "description": "The command string to execute"},
                        **_delay_param()
                    },
                    "required": ["command"]
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "run_powershell",
                "description": "Execute a raw PowerShell command.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "command": {"type": "string", "description": "The PowerShell command string to execute"},
                        **_delay_param()
                    },
                    "required": ["command"]
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "volume_set",
                "description": "Set the system master volume.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "level": {"type": "integer", "description": "Volume 0-100"},
                        **_delay_param()
                    },
                    "required": ["level"]
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "volume_mute",
                "description": "Mute or unmute the system audio.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "mute": {"type": "boolean", "description": "true to mute, false to unmute"},
                        **_delay_param()
                    }
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "change_brightness",
                "description": "Change the screen brightness level.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "level": {"type": "integer", "description": "Brightness 0-100"},
                        **_delay_param()
                    },
                    "required": ["level"]
                }
            }
        },
        # ── Notifications ────────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "notify",
                "description": "Show a Windows toast notification or reminder popup.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "title": {"type": "string", "description": "Notification title"},
                        "message": {"type": "string", "description": "Notification body"},
                        **_delay_param()
                    },
                    "required": ["title", "message"]
                }
            }
        },
        # ── Clipboard ────────────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "clipboard_write",
                "description": "Copy text to the Windows clipboard.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "text": {"type": "string", "description": "Text to copy"},
                        **_delay_param()
                    },
                    "required": ["text"]
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "clipboard_read",
                "description": "Read current text from the Windows clipboard.",
                "parameters": {"type": "object", "properties": {**_delay_param()}}
            }
        },
        # ── Monitoring ───────────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "get_system_stats",
                "description": "Get CPU%, RAM%, disk space, battery%, GPU%, process count, idle time.",
                "parameters": {"type": "object", "properties": {}}
            }
        },
        {
            "type": "function",
            "function": {
                "name": "get_ip_address",
                "description": "Get the local IP address and network adapter info.",
                "parameters": {"type": "object", "properties": {}}
            }
        },
        # ── Window Control ───────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "focus_window",
                "description": "Bring a window to the foreground by its title.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "target": {"type": "string", "description": "Window title (partial match)"},
                        **_delay_param()
                    },
                    "required": ["target"]
                }
            }
        },
        # ── Event Automation ─────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "add_event_rule",
                "description": (
                    "Create an automation rule: 'when X happens → do Y'. "
                    "trigger_type: process_start | process_stop | cpu_high | battery_low | idle_detected | network_change. "
                    "trigger_value: process name for process triggers, threshold number for system triggers. "
                    "actions_json: JSON array of action steps e.g. [{\"action\":\"take_screenshot\"},{\"action\":\"notify\",\"parameters\":{\"title\":\"Alert\",\"message\":\"Discord opened\"}}]"
                ),
                "parameters": {
                    "type": "object",
                    "properties": {
                        "trigger_type":  {"type": "string", "description": "process_start | process_stop | cpu_high | battery_low | idle_detected"},
                        "trigger_value": {"type": "string", "description": "Process name OR numeric threshold"},
                        "actions_json":  {"type": "string", "description": "JSON array string of action steps"},
                        "description":   {"type": "string", "description": "Human-readable rule description"}
                    },
                    "required": ["trigger_type", "trigger_value", "actions_json", "description"]
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "remove_event_rule",
                "description": "Remove an automation rule by its ID.",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "rule_id": {"type": "string", "description": "Rule ID (first 8 chars is enough)"}
                    },
                    "required": ["rule_id"]
                }
            }
        },
        {
            "type": "function",
            "function": {
                "name": "remove_all_event_rules",
                "description": "Remove and delete ALL currently active automation rules. Use this when the user says 'stop all rules' or 'delete active rules'.",
                "parameters": {"type": "object", "properties": {}}
            }
        },
        {
            "type": "function",
            "function": {
                "name": "list_event_rules",
                "description": "List all currently active automation rules.",
                "parameters": {"type": "object", "properties": {}}
            }
        },
        # ── Meta Tools ───────────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "execute_multiple_actions",
                "description": "Execute multiple different actions at the exact same time. Use this ONLY if the user asks for multiple commands in one sentence (e.g., 'play a song AND set volume AND set brightness').",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "actions": {
                            "type": "string",
                            "description": "A JSON array string of the actions to run. Example: [{\"action\": \"play_youtube\", \"parameters\": {\"query\": \"song\"}}, {\"action\": \"volume_set\", \"parameters\": {\"level\": 50}}, {\"action\": \"change_brightness\", \"parameters\": {\"level\": 60}}]"
                        }
                    },
                    "required": ["actions"]
                }
            }
        },
        # ── Memory ───────────────────────────────────────────────────────────
        {
            "type": "function",
            "function": {
                "name": "remember",
                "description": "Persistently store a fact about the user (name, preference, location, etc.).",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "key":   {"type": "string", "description": "Memory key e.g. 'user_name'"},
                        "value": {"type": "string", "description": "Value to store"}
                    },
                    "required": ["key", "value"]
                }
            }
        },
    ]


# ─────────────────────────────────────────────────────────────────────────────
# System Prompt — Planner + Orchestrator, NOT just a tool caller
# ─────────────────────────────────────────────────────────────────────────────

SYSTEM_PROMPT = """You are SARA (Smart Autonomous Runtime Assistant), an intelligent Windows automation runtime and orchestration engine.

You are NOT just a tool caller. You are a PLANNER and ORCHESTRATOR.

## YOUR ROLE
- Understand the user's INTENT (what they REALLY want)
- Build a MULTI-STEP execution plan when needed
- React intelligently to context and memory
- Chain actions that work together as a workflow
- Be proactive: if a task has logical steps, plan all of them

## INTENT CATEGORIES
- media_playback: YouTube, Spotify, music, video, songs
- event_automation: "when X happens", "if Y then Z", automation rules, triggers
- surveillance: screenshots, camera, monitoring, visual capture
- system_control: volume, brightness, processes, apps, power settings
- browser_automation: web search, open websites, navigate
- productivity: clipboard, reminders, scheduling, text operations
- communication: Telegram, notifications, alerts
- monitoring: CPU, RAM, battery, network stats, system health
- reminder: timers, delayed actions, countdowns
- conversational: greetings, questions, casual chat

## PLANNING RULES
1. Always classify the intent first (set the intent field)
2. Use SEMANTIC tools — not raw system calls
3. For media: use play_youtube or open_website
4. For "when X happens" requests → use add_event_rule
5. For delayed actions → use delay_seconds on the step
6. For reminders → use notify with delay_seconds
7. For multi-step tasks → list ALL steps in execution_plan in order. **IF THE USER ASKS FOR MULTIPLE DISTINCT ACTIONS, YOU MUST CALL MULTIPLE TOOLS IN PARALLEL!** Do not skip any requested actions.
8. For conversational/questions → set response_text, leave execution_plan empty
9. Personal info (name, location) → use remember() to store persistently
10. Ambiguous requests → ask for clarification in plain text
11. If you are opening an app and then immediately taking an action on it (e.g. taking a screenshot), add `delay_seconds: 3` to the second action to give the app time to load!

## OUTPUT INSTRUCTIONS
- If the user wants you to take an action, CALL THE APPROPRIATE TOOLS.
- If the user asks for multiple distinct actions (e.g. "play a song AND set volume AND set brightness"), you MUST call the `execute_multiple_actions` tool and bundle all actions inside its `actions` JSON array parameter. Do not just pick one.
- You can call multiple tools if a task requires multiple steps.
- If you need to speak to the user, just output plain text in your response. DO NOT output JSON.
- If you need clarification from the user, ask them in plain text.

## EXAMPLES

User: "play Jungkook Seven on YouTube"
→ (Calls tool `play_youtube` with query="Jungkook Seven")
→ Text: "Playing Jungkook Seven on YouTube! 🎵"

User: "pause the music"
→ (Calls tool `media_play_pause` with action="play_pause")
→ Text: "Paused the media playback."

User: "send alert to close the task manager"
→ (Calls tool `notify` with title="SARA Alert", message="close the task manager")
→ Text: "Alert sent! 🔔"

User: "take screenshot when Discord opens"
→ (Calls tool `add_event_rule` with trigger_type="process_start", trigger_value="discord.exe")
→ Text: "Done! I'll automatically take a screenshot whenever Discord opens. 📸"

User: "close Chrome after 10 minutes"
→ (Calls tool `close_process` with target="chrome.exe", delay_seconds=600)
→ Text: "Got it! I'll close Chrome in 10 minutes. ⏱"

User: "what's my battery?"
→ (Calls tool `get_system_stats`)
→ Text: "Checking your system stats..."

User: "hello, how are you?"
→ Text: "Hey! I'm SARA, your Windows automation assistant. How can I help you today? 😊"
"""


# ─────────────────────────────────────────────────────────────────────────────
# AI API caller
# ─────────────────────────────────────────────────────────────────────────────

def call_ai_api(config: dict, messages: list) -> dict:
    provider = config.get("provider", "openai")
    endpoint = config.get("endpoint", "")
    api_key  = config.get("api_key", "")
    model    = config.get("model", "gpt-4o-mini")
    max_tok  = config.get("max_tokens", 1024)
    temp     = config.get("temperature", 0.7)
    timeout  = config.get("timeout_seconds", 30)

    if not endpoint:
        endpoints = {
            "openai": "https://api.openai.com/v1/chat/completions",
            "ollama": "http://localhost:11434/v1/chat/completions",
            "groq":   "https://api.groq.com/openai/v1/chat/completions",
            "claude": "https://api.anthropic.com/v1/messages",
            "gemini": "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions",
        }
        endpoint = endpoints.get(provider, "")
        if not endpoint:
            return {"success": False, "error": f"Unknown provider: {provider}"}

    body = {
        "model":       model,
        "messages":    messages,
        "tools":       build_tools(),
        "max_tokens":  max_tok,
        "temperature": temp,
    }

    headers = {"Content-Type": "application/json", "User-Agent": "SARA/2.0"}
    if api_key:
        headers["Authorization"] = f"Bearer {api_key}"

    req = urllib.request.Request(
        endpoint,
        data=json.dumps(body).encode("utf-8"),
        headers=headers,
        method="POST",
    )

    raw = ""
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8")
            result = json.loads(raw)
    except urllib.error.HTTPError as e:
        body_err = e.read().decode("utf-8", errors="replace")[:2000]
        return {"success": False, "error": f"HTTP {e.code} {e.reason}: {body_err}"}
    except Exception as e:
        return {"success": False, "error": f"{type(e).__name__}: {str(e)}"}

    # ── Parse response → execution_plan ──────────────────────────────────────
    try:
        choice  = result["choices"][0]["message"]
        content = choice.get("content") or ""
        steps   = []

        tool_calls = choice.get("tool_calls") or []

        # Legacy function_call fallback
        if not tool_calls and "function_call" in choice:
            fc = choice["function_call"]
            tool_calls = [{"function": {
                "name":      fc["name"],
                "arguments": fc.get("arguments", "{}")
            }}]

        for tc in tool_calls:
            fn   = tc["function"]
            name = fn["name"]
            args = fn.get("arguments", "{}")
            if isinstance(args, str):
                try:    args = json.loads(args)
                except: args = {}

            if name == "execute_multiple_actions":
                sub_actions_str = args.get("actions", "[]")
                try:
                    sub_actions = json.loads(sub_actions_str)
                    for sub in sub_actions:
                        sub_name = sub.get("action", "")
                        if not sub_name: continue
                        sub_params = sub.get("parameters", {})
                        steps.append({
                            "action": sub_name,
                            "target": sub_params.get("target", sub_params.get("url", sub_params.get("query", ""))),
                            "parameters": sub_params,
                            "description": sub.get("description", f"Execute {sub_name}")
                        })
                except Exception as e:
                    pass
                continue

            # Build a canonical step
            step = {
                "action":      name,
                "target":      args.get("target", args.get("url", args.get("query", ""))),
                "parameters":  args,
                "description": args.get("description", f"Execute {name}"),
            }
            steps.append(step)

        return {
            "success":               True,
            "intent":                _classify_intent_from_steps(steps, content),
            "execution_plan":        steps,
            "response_text":         content,
            "needs_clarification":   False,
            "clarification_question": ""
        }

    except Exception as e:
        return {"success": False, "error": f"Parse error: {e} | raw={raw[:500]}"}


# ─────────────────────────────────────────────────────────────────────────────
# Simple client-side intent label from tool names
# ─────────────────────────────────────────────────────────────────────────────

def _classify_intent_from_steps(steps: list, content: str) -> str:
    if not steps:
        return "conversational"
    action = steps[0].get("action", "")
    mapping = {
        "play_youtube":      "media_playback",
        "media_play_pause":  "media_playback",
        "media_next":        "media_playback",
        "media_prev":        "media_playback",
        "media_stop":        "media_playback",
        "search_google":     "browser_automation",
        "open_website":      "browser_automation",
        "open_app":          "system_control",
        "close_process":     "system_control",
        "take_screenshot":   "surveillance",
        "capture_camera":    "surveillance",
        "volume_set":        "system_control",
        "volume_mute":       "system_control",
        "change_brightness": "system_control",
        "notify":            "reminder",
        "clipboard_write":   "productivity",
        "clipboard_read":    "productivity",
        "get_system_stats":  "monitoring",
        "get_ip_address":    "monitoring",
        "focus_window":      "system_control",
        "add_event_rule":    "event_automation",
        "remove_event_rule": "event_automation",
        "remove_all_event_rules": "event_automation",
        "list_event_rules":  "event_automation",
        "execute_multiple_actions": "system_control",
        "remember":          "productivity",
    }
    return mapping.get(action, "unknown")


# ─────────────────────────────────────────────────────────────────────────────
# Main parsing logic
# ─────────────────────────────────────────────────────────────────────────────

def parse_command(data: dict) -> dict:
    user_message    = data.get("user_message", "")
    config          = data.get("config", {})
    session_context = data.get("session_context", {})
    memory_context  = data.get("memory_context", "")

    # Build system prompt with optional memory and intent hint
    system = SYSTEM_PROMPT
    if memory_context:
        system += f"\n\n## KNOWN USER CONTEXT\n{memory_context}"

    messages = [{"role": "system", "content": system}]

    # Inject recent command history for context awareness
    recent = (session_context or {}).get("recent_commands", [])
    if recent:
        hist_txt = "Previous execution actions (for context):\n"
        for cmd in recent[-3:]:
            hist_txt += f"- {json.dumps(cmd)}\n"
        messages.append({"role": "system", "content": hist_txt})

    # Inject conversation history
    chat_hist = (session_context or {}).get("history", [])
    if chat_hist:
        for msg in chat_hist:
            messages.append({"role": msg.get("role", "user"), "content": msg.get("content", "")})

    messages.append({"role": "user", "content": user_message})

    return call_ai_api(config, messages)


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────

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
