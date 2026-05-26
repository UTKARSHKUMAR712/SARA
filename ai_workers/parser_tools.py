def _delay_param():
    return {'delay_seconds': {'type': 'integer', 'description': 'Delay in seconds before execution'}}

def build_tools():
    return [
        {'type': 'function', 'function': {'name': 'media_command', 'description': 'Media (action=spotify_play, play_youtube, etc)', 'parameters': {'type': 'object', 'properties': {'action': {'type': 'string'}, 'query': {'type': 'string'}, 'value': {'type': 'integer'}}, 'required': ['action']}}},
        {'type': 'function', 'function': {'name': 'search_google', 'description': 'Search', 'parameters': {'type': 'object', 'properties': {'query': {'type': 'string'}, **_delay_param()}, 'required': ['query']}}},
        {'type': 'function', 'function': {'name': 'open_website', 'description': 'Open', 'parameters': {'type': 'object', 'properties': {'url': {'type': 'string'}, **_delay_param()}, 'required': ['url']}}},
        {'type': 'function', 'function': {'name': 'open_app', 'description': 'App', 'parameters': {'type': 'object', 'properties': {'target': {'type': 'string'}, **_delay_param()}, 'required': ['target']}}},
        {'type': 'function', 'function': {'name': 'close_process', 'description': 'Close', 'parameters': {'type': 'object', 'properties': {'target': {'type': 'string'}, **_delay_param()}, 'required': ['target']}}},
        {'type': 'function', 'function': {'name': 'take_screenshot', 'description': 'Screenshot', 'parameters': {'type': 'object', 'properties': {}}}},
        {'type': 'function', 'function': {'name': 'capture_camera', 'description': 'Camera', 'parameters': {'type': 'object', 'properties': {'device_index': {'type': 'integer'}}, 'required': []}}},
        {'type': 'function', 'function': {'name': 'run_cmd', 'description': 'CMD', 'parameters': {'type': 'object', 'properties': {'command': {'type': 'string'}}, 'required': ['command']}}},
        {'type': 'function', 'function': {'name': 'run_powershell', 'description': 'PS', 'parameters': {'type': 'object', 'properties': {'command': {'type': 'string'}}, 'required': ['command']}}},
        {'type': 'function', 'function': {'name': 'task_complete', 'description': 'Mark task done', 'parameters': {'type': 'object', 'properties': {}}}},
        {'type': 'function', 'function': {'name': 'write_file', 'description': 'Write file', 'parameters': {'type': 'object', 'properties': {'path': {'type': 'string'}, 'content': {'type': 'string'}, **_delay_param()}, 'required': ['path', 'content']}}},
        {'type': 'function', 'function': {'name': 'read_file', 'description': 'Read file', 'parameters': {'type': 'object', 'properties': {'path': {'type': 'string'}}, 'required': ['path']}}},
        {'type': 'function', 'function': {'name': 'list_dir', 'description': 'List directory', 'parameters': {'type': 'object', 'properties': {'path': {'type': 'string'}}, 'required': ['path']}}},
        {'type': 'function', 'function': {'name': 'lock_pc', 'description': 'Lock', 'parameters': {'type': 'object', 'properties': {**_delay_param()}}}},
        {'type': 'function', 'function': {'name': 'volume_set', 'description': 'Vol', 'parameters': {'type': 'object', 'properties': {'level': {'type': 'integer'}}, 'required': ['level']}}},
        {'type': 'function', 'function': {'name': 'volume_mute', 'description': 'Mute', 'parameters': {'type': 'object', 'properties': {'mute': {'type': 'boolean'}}, 'required': []}}},
        {'type': 'function', 'function': {'name': 'change_brightness', 'description': 'Bright', 'parameters': {'type': 'object', 'properties': {'level': {'type': 'integer'}}, 'required': ['level']}}},
        {'type': 'function', 'function': {'name': 'notify', 'description': 'Notify', 'parameters': {'type': 'object', 'properties': {'title': {'type': 'string'}, 'message': {'type': 'string'}}, 'required': ['title', 'message']}}},
        {'type': 'function', 'function': {'name': 'clipboard_write', 'description': 'Copy', 'parameters': {'type': 'object', 'properties': {'text': {'type': 'string'}}, 'required': ['text']}}},
        {'type': 'function', 'function': {'name': 'clipboard_read', 'description': 'Paste', 'parameters': {'type': 'object', 'properties': {}}}},
        {'type': 'function', 'function': {'name': 'get_system_stats', 'description': 'Stats', 'parameters': {'type': 'object', 'properties': {}}}},
        {'type': 'function', 'function': {'name': 'get_ip_address', 'description': 'IP', 'parameters': {'type': 'object', 'properties': {}}}},
        {'type': 'function', 'function': {'name': 'focus_window', 'description': 'Focus', 'parameters': {'type': 'object', 'properties': {'target': {'type': 'string'}}, 'required': ['target']}}},
        {'type': 'function', 'function': {'name': 'add_event_rule', 'description': 'Add rule', 'parameters': {'type': 'object', 'properties': {'trigger_type': {'type': 'string'}, 'trigger_value': {'type': 'string'}, 'actions_json': {'type': 'string'}, 'description': {'type': 'string'}}, 'required': ['trigger_type', 'trigger_value', 'actions_json', 'description']}}},
        {'type': 'function', 'function': {'name': 'remove_event_rule', 'description': 'Rem rule', 'parameters': {'type': 'object', 'properties': {'rule_id': {'type': 'string'}}, 'required': ['rule_id']}}},
        {'type': 'function', 'function': {'name': 'remove_all_event_rules', 'description': 'Rem all', 'parameters': {'type': 'object', 'properties': {}}}},
        {'type': 'function', 'function': {'name': 'list_event_rules', 'description': 'List', 'parameters': {'type': 'object', 'properties': {}}}},
        {'type': 'function', 'function': {'name': 'execute_multiple_actions', 'description': 'Multiple', 'parameters': {'type': 'object', 'properties': {'actions': {'type': 'string'}}, 'required': ['actions']}}},
        {'type': 'function', 'function': {'name': 'remember', 'description': 'Store', 'parameters': {'type': 'object', 'properties': {'key': {'type': 'string'}, 'value': {'type': 'string'}}, 'required': ['key', 'value']}}}
    ]

def get_tools_for_category(category, all_python_tools, dynamic_tools):
    selected = []
    mapping = {
        "media": ["media_command"],
        "system": ["open_", "close_", "volume", "change", "focus", "get_system", "get_ip", "run_", "write_", "lock_", "read_", "list_"],
        "automation": ["add_event", "remove_event", "list_event", "notify", "run_"],
        "browser": ["search", "open_website", "web_search", "duckduckgo", "run_"],
        "surveillance": ["take_", "capture_"],
        "misc": ["execute_multiple", "clipboard_", "remember", "run_", "task_complete"]
    }
    prefixes = mapping.get(category, [])
    
    # Filter out individual media tools from dynamic tools to save tokens
    filtered_dynamic = []
    for t in dynamic_tools:
        name = t.get("function", {}).get("name", "")
        if name.startswith("spotify_") or name.startswith("media_") or name == "play_youtube":
            continue
        filtered_dynamic.append(t)
        
    merged = { t["function"]["name"]: t for t in filtered_dynamic + all_python_tools }
    if category == "chat": return []
    for name, tool in merged.items():
        if any(name.startswith(p) for p in prefixes):
            selected.append(tool)
    if "execute_multiple_actions" in merged: selected.append(merged["execute_multiple_actions"])
    if "media_command" in merged and merged["media_command"] not in selected: selected.append(merged["media_command"])
    return selected if selected else list(merged.values())
