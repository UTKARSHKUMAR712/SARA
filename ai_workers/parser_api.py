import json
import os
import urllib.request
import urllib.error

SYSTEM_PROMPT = """You are SARA, an autonomous OS agent. You have FULL unrestricted access to this PC.

TOOL RULES — follow exactly:
- NEVER use run_cmd or run_powershell to create files/folders. Use write_file ONLY.
- write_file(path, content): path is relative to C:\\Users\\utkarsh_kumar\\Desktop (e.g. test/bakery.html). It auto-creates parent folders.
- read_file(path): reads file. path relative to Desktop.
- list_dir(path): lists directory. path relative to Desktop.
- open_app(target): target = full exe path or app name like 'chrome'.
- media_command: action = 'spotify_play', 'play_youtube'.
- run_powershell / run_cmd: ONLY for system queries (get CPU, list processes, etc). Working dir = Desktop.

AGENTIC LOOP:
- You execute ONE tool per iteration. The system loops automatically, feeding you each tool's output.
- After each tool output, decide your NEXT action based on the result.
- When ALL tasks in the user request are 100% done, call task_complete.
- NEVER repeat a successful action. Check history before acting.
- NEVER make more than 1 tool call per response.
"""


# ─────────────────────────────────────────────────────────────────────────────
# AI API caller
# ─────────────────────────────────────────────────────────────────────────────

def call_gemini_api(config: dict, messages: list, final_tools: list) -> dict:
    import json, urllib.request, urllib.error
    
    model = config.get("model", "gemini-2.0-flash")
    api_key = config.get("api_key", "")
    timeout = config.get("timeout_seconds", 120)
    
    # We use alt=sse to get Server-Sent Events like OpenAI
    endpoint = f"https://generativelanguage.googleapis.com/v1beta/models/{model}:streamGenerateContent?key={api_key}&alt=sse"
    
    system_text = ""
    contents = []
    for m in messages:
        role = m.get("role", "user")
        text = m.get("content", "")
        if role == "system":
            system_text += text + "\n"
        elif role == "user":
            contents.append({"role": "user", "parts": [{"text": text}]})
        elif role == "assistant":
            if text:
                contents.append({"role": "model", "parts": [{"text": text}]})
            if m.get("tool_calls"):
                for tc in m["tool_calls"]:
                    func = tc.get("function", {})
                    args_str = func.get("arguments", "{}")
                    try:
                        args = json.loads(args_str)
                    except:
                        args = {}
                    contents.append({
                        "role": "model", 
                        "parts": [{"functionCall": {"name": func.get("name"), "args": args}}]
                    })
        elif role == "tool":
            # For Gemini, tool responses look like:
            # {"role": "function", "parts": [{"functionResponse": {"name": name, "response": {"result": content}}}]}
            pass # SARA currently doesn't send tool history in parser, it's a 1-shot parser usually

    funcs = []
    for t in final_tools:
        if t.get("type") == "function":
            # Gemini function parameters must have "type": "object" at root
            f = dict(t["function"])  # shallow copy to avoid mutating source
            if "parameters" in f and "type" not in f["parameters"]:
                f["parameters"]["type"] = "object"
            funcs.append(f)

    # Build Gemini tools array
    # IMPORTANT: only add googleSearch/codeExecution when NOT doing a cognitive
    # call (cognitive calls need pure JSON text responses, not code/search output)
    gemini_tools = []
    if funcs:
        # Real tool call — include function declarations
        gemini_tools.append({"functionDeclarations": funcs})
        # Only add search/code on full executor calls (not cognitive sub-calls)
        # Detect cognitive calls by checking if this is a single-message no-tool call
        if len(messages) > 2:
            gemini_tools.append({"googleSearch": {}})
    # If funcs is empty, omit tools entirely → forces plain text response

    body = {
        "contents": contents,
    }
    if gemini_tools:
        body["tools"] = gemini_tools
        # Force function-calling when tools are present — prevents text-only responses
        body["tool_config"] = {
            "function_calling_config": {"mode": "ANY"}
        }
    if system_text:
        body["systemInstruction"] = {"parts": [{"text": system_text}]}

    # Debug: dump request to file
    try:
        import os
        debug_path = r"C:\Users\utkarsh_kumar\Desktop\sara\debug_gemini.json"
        with open(debug_path, "w", encoding="utf-8") as df:
            df.write(json.dumps({"request_body": body, "endpoint": endpoint[:80]}, indent=2))
    except Exception:
        pass

    req = urllib.request.Request(
        endpoint,
        data=json.dumps(body).encode("utf-8"),
        headers={"Content-Type": "application/json", "User-Agent": "SARA/2.0"},
        method="POST"
    )
    full_content = ""
    tool_calls_dict = {}
    raw_lines = []

    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            for line in resp:
                line = line.decode("utf-8").strip()
                raw_lines.append(line)
                if line.startswith("data: "):
                    data_str = line[6:]
                    if data_str == "[DONE]":
                        break
                    try:
                        chunk = json.loads(data_str)
                        if not chunk.get("candidates"):
                            continue
                        candidate = chunk["candidates"][0]
                        parts = candidate.get("content", {}).get("parts", [])
                        for part in parts:
                            if "text" in part:
                                full_content += part["text"]
                            if "functionCall" in part:
                                fcall = part["functionCall"]
                                fname = fcall.get("name", "")
                                fargs = json.dumps(fcall.get("args", {}))
                                idx = len(tool_calls_dict)
                                tool_calls_dict[idx] = {
                                    "id": f"call_gemini_{idx}",
                                    "type": "function",
                                    "function": {"name": fname, "arguments": fargs}
                                }
                    except Exception:
                        pass
    except urllib.error.HTTPError as e:
        body_err = e.read().decode("utf-8", errors="replace")[:3000]
        # Write error to debug file
        try:
            with open(r"C:\Users\utkarsh_kumar\Desktop\sara\debug_gemini_error.txt", "w", encoding="utf-8") as ef:
                ef.write(f"HTTP {e.code}: {body_err}")
        except Exception:
            pass
        return {"success": False, "error": f"Gemini HTTP {e.code}: {body_err}"}
    except Exception as e:
        return {"success": False, "error": str(e)}

    # Debug: dump raw response
    try:
        with open(r"C:\Users\utkarsh_kumar\Desktop\sara\debug_gemini_response.txt", "w", encoding="utf-8") as rf:
            rf.write("FULL_CONTENT:\n" + full_content + "\n\nTOOL_CALLS:\n" + json.dumps(tool_calls_dict, indent=2) + "\n\nRAW_LINES (last 20):\n" + "\n".join(raw_lines[-20:]))
    except Exception:
        pass

    tool_calls_list = [tool_calls_dict[i] for i in sorted(tool_calls_dict.keys())]

    return {
        "choices": [{
            "message": {
                "role": "assistant",
                "content": full_content,
                "tool_calls": tool_calls_list if tool_calls_list else None
            }
        }]
    }

def call_ai_api(config: dict, messages: list, final_tools: list) -> dict:
    provider = config.get("provider", "openai")
    endpoint = config.get("endpoint", "")
    api_key  = config.get("api_key", "")
    model    = config.get("model", "gpt-4o-mini")
    max_tok  = config.get("max_tokens", 2048)
    if max_tok > 2048:
        max_tok = 2048  # Cap it to avoid rate limits on free providers like Groq
    temp     = config.get("temperature", 0.7)
    timeout  = config.get("timeout_seconds", 120)

    if provider.lower() == "gemini":
        return call_gemini_api(config, messages, final_tools)

    if not endpoint:
        endpoints = {
            "openai": "https://api.openai.com/v1/chat/completions",
            "ollama": "http://localhost:11434/v1/chat/completions",
            "groq":   "https://api.groq.com/openai/v1/chat/completions",
            "claude": "https://api.anthropic.com/v1/messages",
        }
        endpoint = endpoints.get(provider, "")
        if not endpoint:
            return {"success": False, "error": f"Unknown provider: {provider}"}

    body = {
        "model":       model,
        "messages":    messages,
        "max_tokens":  max_tok,
        "temperature": temp,
        "stream":      False,  # Set to False to get usage stats reliably for testing
    }
    if final_tools:
        body["tools"] = final_tools

    # Load custom kwargs if available to support any future endpoints without rebuilding C++
    kwargs_path = os.path.join(os.path.dirname(__file__), "provider_kwargs.json")
    if os.path.exists(kwargs_path):
        try:
            with open(kwargs_path, "r", encoding="utf-8") as f:
                custom_kwargs = json.load(f)
            # Match by model name or provider
            if model in custom_kwargs:
                body.update(custom_kwargs[model])
            elif provider in custom_kwargs:
                body.update(custom_kwargs[provider])
        except Exception:
            pass

    thinking_enabled = config.get("thinking_enabled", False)

    headers = {"Content-Type": "application/json", "User-Agent": "SARA/2.0"}
    if api_key:
        headers["Authorization"] = f"Bearer {api_key}"

    # We will attempt the request. If it fails due to chat_template_kwargs, we retry without it.
    attempts = 2
    use_kwargs = True
    result = None

    for attempt in range(attempts):
        if use_kwargs:
            if "chat_template_kwargs" not in body:
                body["chat_template_kwargs"] = {}
            body["chat_template_kwargs"]["thinking"] = bool(thinking_enabled)
            if thinking_enabled:
                body["chat_template_kwargs"]["reasoning_effort"] = "high"
        else:
            if "chat_template_kwargs" in body:
                del body["chat_template_kwargs"]

        req = urllib.request.Request(
            endpoint,
            data=json.dumps(body).encode("utf-8"),
            headers=headers,
            method="POST",
        )

        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                if body.get("stream"):
                    full_content = ""
                    tool_calls_dict = {}
                    for line in resp:
                        line = line.decode("utf-8").strip()
                        if line.startswith("data: "):
                            data_str = line[6:]
                            if data_str == "[DONE]":
                                break
                            try:
                                chunk = json.loads(data_str)
                                if not chunk.get("choices"):
                                    continue
                                delta = chunk["choices"][0].get("delta", {})
                                
                                c_content = delta.get("content")
                                if c_content is not None:
                                    full_content += c_content
                                    
                                t_calls = delta.get("tool_calls")
                                if t_calls:
                                    for tc in t_calls:
                                        idx = tc.get("index", 0)
                                        if idx not in tool_calls_dict:
                                            t_id = tc.get("id", f"call_{idx}")
                                            tool_calls_dict[idx] = {"id": t_id, "type": "function", "function": {"name": "", "arguments": ""}}
                                        
                                        if "function" in tc:
                                            if "name" in tc["function"] and tc["function"]["name"]:
                                                tool_calls_dict[idx]["function"]["name"] += tc["function"]["name"]
                                            if "arguments" in tc["function"] and tc["function"]["arguments"]:
                                                tool_calls_dict[idx]["function"]["arguments"] += tc["function"]["arguments"]
                            except:
                                pass
                    
                    tool_calls_list = [tool_calls_dict[i] for i in sorted(tool_calls_dict.keys())]
                    result = {
                        "choices": [{
                            "message": {
                                "role": "assistant",
                                "content": full_content,
                                "tool_calls": tool_calls_list if tool_calls_list else None
                            }
                        }]
                    }
                else:
                    raw = resp.read().decode("utf-8")
                    result = json.loads(raw)
                break  # Success!

        except urllib.error.HTTPError as e:
            body_err = e.read().decode("utf-8", errors="replace")
            # If 400 Bad Request and mentions kwargs/thinking/parameters, retry without kwargs
            if attempt == 0 and e.code == 400 and ("chat_template_kwargs" in body_err or "Unknown parameter" in body_err or "thinking" in body_err or "extra_body" in body_err):
                use_kwargs = False
                continue
            tool_names = [t["function"]["name"] for t in final_tools] if final_tools else []
            return {"success": False, "error": f"HTTP {e.code} {e.reason}: {body_err[:2000]} | Tools Sent: {tool_names}"}
        except Exception as e:
            return {"success": False, "error": f"{type(e).__name__}: {str(e)}"}

    # ── Parse response → execution_plan ──────────────────────────────────────
    try:
        choice  = result["choices"][0]["message"]
        tool_calls = choice.get("tool_calls") or []
        content = choice.get("content") or ""

        # Legacy function_call fallback
        if not tool_calls and "function_call" in choice:
            fc = choice["function_call"]
            tool_calls = [{"function": fc}]

        # Forcefully discard any conversational text if tools are used to prevent hallucinations
        if tool_calls:
            content = ""

        # [DEBUG] Append usage and payload for testing
        usage = result.get("usage", {})
        if usage:
            content += f"\n\n[DEBUG] Tools Loaded: {len(final_tools)} | Input Tokens: {usage.get('prompt_tokens', '?')} | Output Tokens: {usage.get('completion_tokens', '?')}"
        else:
            content += f"\n\n[DEBUG] Tools Loaded: {len(final_tools)} | Input Length: {len(json.dumps(body))} chars"
        
        steps   = []

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

            if name == "media_command":
                sub_name = args.get("action", "")
                if not sub_name: continue
                sub_params = {}
                if "query" in args: sub_params["query"] = args["query"]
                if "value" in args:
                    if "volume" in sub_name: sub_params["level"] = args["value"]
                    else: sub_params["delay_seconds"] = args["value"]
                if "shuffle" in sub_name or "repeat" in sub_name:
                    sub_params["mode"] = args.get("query", "")
                step = {
                    "action": sub_name,
                    "target": args.get("query", ""),
                    "parameters": sub_params,
                    "description": f"Execute {sub_name}"
                }
                steps.append(step)
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
            "clarification_question": "",
            "needs_continue":        (not any(s.get("action") == "task_complete" for s in steps)) and len(steps) > 0,
            "raw_choice":            choice
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
        "media_command":     "media_playback",
        "play_youtube":      "media_playback",
        "spotify_play":      "media_playback",
        "spotify_pause":     "media_playback",
        "spotify_resume":    "media_playback",
        "spotify_next":      "media_playback",
        "spotify_prev":      "media_playback",
        "spotify_volume":    "media_playback",
        "spotify_get_status": "media_playback",
        "spotify_dock":      "media_playback",
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


