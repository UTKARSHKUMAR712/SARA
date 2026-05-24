# SARA — AI Handoff Reference
## Complete Architecture, API & Implementation Guide for AI Agents

---

# 1. PROJECT OVERVIEW

SARA (Smart Autonomous Runtime Assistant) is a **lightweight native Windows AI assistant** built as:
- A C++17/20 daemon (`sara_agent.exe`) — zero GUI dependency
- An optional GUI controller (`sara_gui.exe`) — communicates via IPC
- An AI orchestration layer (temporary Python workers, multi-provider)
- A WinAPI automation runtime (keyboard, mouse, processes, screenshots, etc.)
- A remote-controllable agent (Telegram first, Discord/WhatsApp later)
- A plugin ecosystem (executable/IPC-based plugins)

**File size limit**: max 500 lines per source file (split at 400 lines).

---

# 2. DIRECTORY STRUCTURE

```
SARA/
├── sara_agent/           # Core runtime (C++17/20)
│   ├── src/              # Source files
│   ├── include/          # Headers
│   └── CMakeLists.txt    # Build config
├── sara_gui/             # Optional GUI (framework TBD)
├── shared/               # Shared headers/schemas between agent + GUI
├── plugins/              # Plugin ecosystem
├── ai_workers/           # Python AI worker scripts
├── configs/              # JSON configuration files
├── logs/                 # Runtime logs
├── data/                 # SQLite database storage
└── docs/                 # Additional documentation
```

---

# 3. CORE RUNTIME — sara_agent.exe

## 3.1 Component Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        sara_agent.exe                           │
│                                                                 │
│  ┌──────────┐  ┌──────────────┐  ┌──────────┐  ┌────────────┐ │
│  │  Logger   │  │ ConfigManager│  │ Scheduler│  │ IPCServer  │ │
│  └──────────┘  └──────────────┘  └──────────┘  └────────────┘ │
│                                                                 │
│  ┌───────────────┐  ┌──────────────┐  ┌───────────────────┐   │
│  │ TelegramGw     │  │ WinAPIExec   │  │ AIWorkerLauncher  │   │
│  └───────────────┘  └──────────────┘  └───────────────────┘   │
│                                                                 │
│  ┌──────────┐  ┌──────────────┐  ┌───────────────────┐        │
│  │ Validator│  │ PermissionMgr│  │ SessionManager    │        │
│  └──────────┘  └──────────────┘  └───────────────────┘        │
└─────────────────────────────────────────────────────────────────┘
```

## 3.2 Modules & Responsibilities

### Logger
- **File**: `sara_agent/src/Logger.cpp` / `sara_agent/include/Logger.h`
- **Levels**: DEBUG, INFO, WARNING, ERROR
- **Output**: File + optional console
- **Features**: Timestamps, log rotation (future), configurable level

### ConfigManager
- **File**: `sara_agent/src/ConfigManager.cpp`
- **Format**: JSON (`configs/config.json`)
- **Contents**: API keys, Telegram token, endpoints, permissions, model settings, provider config

### Scheduler
- **File**: `sara_agent/src/Scheduler.cpp`
- **Loop**: check_tasks() every 500ms sleep
- **Task types**: delayed, repeating, conditional
- **Persistence**: SQLite (`data/scheduler.db`)

### IPC Server
- **File**: `sara_agent/src/IPCServer.cpp`
- **Method**: Named Pipes (primary), localhost sockets (fallback)
- **Purpose**: GUI ↔ Agent, Plugins ↔ Agent, AI Worker ↔ Agent

### Telegram Gateway
- **File**: `sara_agent/src/TelegramGateway.cpp`
- **Mode**: Polling (HTTP-based, lightweight)
- **Flow**: Telegram → Gateway → AI Parser → Scheduler/Executor

### WinAPI Executor
- **File**: `sara_agent/src/WinAPIExecutor.cpp`
- **APIs used**: CreateProcess, SendInput, ShellExecute, TerminateProcess, EnumWindows, BitBlt, etc.
- **Capabilities**: open/close apps, keyboard/mouse automation, screenshots, notifications, CMD/PowerShell execution

### AI Worker Launcher
- **File**: `sara_agent/src/AIWorkerLauncher.cpp`
- **Behavior**: Start temporary Python process → pass prompt → receive structured JSON → terminate
- **Python NEVER stays running permanently**

### Action Validator
- **File**: `sara_agent/src/ActionValidator.cpp`
- **Checks**: allowed commands, permission level, parameter validation, schema compliance

### Permission Manager
- **File**: `sara_agent/src/PermissionManager.cpp`
- **Levels**: LOW (auto), MEDIUM (confirm), HIGH (approval required)
- **Storage**: SQLite permissions table + config overrides

### Session Manager
- **File**: `sara_agent/src/SessionManager.cpp`
- **Purpose**: Track conversation context for ambiguous commands ("close it after 5 min")
- **Features**: Auto-expiration, memory limits, inactivity cleanup

---

# 4. APIs & SCHEMAS (Exposed for AI Consumption)

## 4.1 Internal Standard Action Schema

Every component uses this exact schema. This is the UNIVERSAL action format.

```json
{
  "id": "uuid-v4-string",
  "source": "telegram|gui|discord|plugin|scheduler|ai_worker",
  "type": "task|event|plugin|system",
  "action": "close_process|open_app|take_screenshot|notify|send_keys|run_cmd|run_powershell|volume_set|clipboard_write|etc",
  "target": "Discord.exe|chrome|etc (optional)",
  "parameters": {},
  "requires_approval": true|false,
  "created_at": 1712345678,
  "execute_at": 1712345678,
  "repeat": false|true,
  "interval_seconds": 0,
  "priority": "low|normal|high|critical"
}
```

### All Defined Actions

| Action | Target | Parameters | Risk |
|--------|--------|-----------|------|
| `open_app` | path/exe name | `{ args: "" }` | LOW |
| `close_process` | process name | `{ force: false }` | MEDIUM |
| `take_screenshot` | — | `{ delay: 0, format: "png" }` | LOW |
| `send_keys` | — | `{ keys: "string" }` | MEDIUM |
| `mouse_move` | — | `{ x: 0, y: 0 }` | MEDIUM |
| `mouse_click` | — | `{ button: "left", x: 0, y: 0 }` | MEDIUM |
| `notify` | — | `{ title: "", message: "" }` | LOW |
| `volume_set` | — | `{ level: 50 }` | LOW |
| `volume_mute` | — | `{ mute: true }` | LOW |
| `clipboard_write` | — | `{ text: "" }` | LOW |
| `clipboard_read` | — | — | MEDIUM |
| `run_cmd` | — | `{ command: "", timeout: 30 }` | HIGH |
| `run_powershell` | — | `{ command: "", timeout: 30 }` | HIGH |
| `run_bat` | — | `{ content: "", timeout: 30 }` | HIGH |
| `focus_window` | window title | — | LOW |
| `enum_windows` | — | `{ filter: "" }` | LOW |
| `get_system_stats` | — | `{ type: "cpu|ram|disk|gpu|net" }` | LOW |
| `schedule_task` | — | (uses action schema) | MEDIUM |
| `cancel_task` | task ID | — | LOW |
| `list_tasks` | — | — | LOW |
| `get_plugin_info` | plugin name | — | LOW |
| `reload_config` | — | — | MEDIUM |

## 4.2 IPC Protocol

### Message Format (Named Pipes / localhost sockets)

```json
{
  "source": "gui|agent|plugin|ai_worker",
  "destination": "agent|gui|plugin",
  "type": "command|response|event|status|error",
  "request_id": "uuid-v4-string",
  "payload": {}
}
```

### Message Types

| Type | Direction | Purpose |
|------|-----------|---------|
| `command` | → agent | Execute an action |
| `response` | ← agent | Reply to command |
| `event` | bidirectional | Broadcast notifications |
| `status` | ← agent | Runtime health/heartbeat |
| `error` | ← agent | Failure details |

## 4.3 Event System

```json
{
  "event": "MessageReceived|TaskCreated|TaskCompleted|TaskCancelled|PluginLoaded|PluginFailed|CPUHigh|NetworkSpike|PermissionRequested|ScreenshotTaken|ConfigChanged|SystemWarning",
  "timestamp": 1712345678,
  "source": "scheduler|telegram|gui|plugin|system_monitor",
  "priority": "critical|high|normal|low",
  "payload": {}
}
```

### All Events

| Event | Priority | Payload |
|-------|----------|---------|
| `MessageReceived` | normal | `{ platform, text, chat_id, user }` |
| `TaskCreated` | normal | action object |
| `TaskCompleted` | normal | `{ task_id, result }` |
| `TaskCancelled` | low | `{ task_id, reason }` |
| `PluginLoaded` | normal | `{ plugin_name, version }` |
| `PluginFailed` | high | `{ plugin_name, error }` |
| `CPUHigh` | high | `{ cpu_percent, threshold }` |
| `NetworkSpike` | high | `{ bandwidth, threshold }` |
| `PermissionRequested` | critical | `{ action, user, risk_level }` |
| `ScreenshotTaken` | low | `{ path, size }` |
| `ConfigChanged` | normal | `{ key, old_value, new_value }` |
| `SystemWarning` | high | `{ component, message }` |

## 4.4 Plugin Manifest Format

```json
{
  "name": "netsense",
  "version": "1.0.0",
  "description": "Network diagnostics plugin",
  "author": "",
  "permissions": ["network_read"],
  "commands": ["network_stats", "ping_test", "bandwidth_check"],
  "type": "executable|api|python",
  "entry": "plugin.exe|connector.py",
  "ipc_port": 0,
  "config": {}
}
```

### All Permission Types

```
network_read, network_write, system_read, system_write,
shell_execute, screenshot_access, filesystem_read, filesystem_write,
clipboard_access, audio_control, process_manage, registry_access
```

## 4.5 Session Memory Structure

```json
{
  "session_id": "uuid-string",
  "platform": "telegram|discord|gui",
  "chat_id": "user-or-chat-identifier",
  "last_targets": ["Discord.exe", "chrome"],
  "recent_commands": [
    { "action": "close_process", "target": "Discord.exe", "timestamp": 0 }
  ],
  "created_at": 0,
  "last_activity": 0
}
```

**Cleanup**: Expire after N minutes of inactivity, max N entries in recent_commands.

## 4.6 AI Provider Configuration

```json
{
  "provider": "openai|claude|gemini|groq|nvidia_nim|ollama|custom",
  "endpoint": "https://api.openai.com/v1/chat/completions",
  "api_key": "encrypted-or-plaintext",
  "model": "gpt-4o-mini|claude-3-haiku|gemini-pro|etc",
  "additional_headers": {},
  "max_tokens": 1024,
  "temperature": 0.7,
  "timeout_seconds": 30
}
```

### Internal Normalized Message Format

```json
// Input (all providers convert TO this)
{ "role": "user|system|assistant", "content": "text" }

// Output (all providers convert FROM this)
{ "success": true|false, "structured_action": { ... } }
```

### AI Router Routing Table

| Task Type | Preferred Provider | Fallback |
|-----------|-------------------|----------|
| coding/debug | Claude | OpenAI |
| fast/simple | Groq | Ollama |
| local/offline | Ollama | — |
| reasoning | Gemini | OpenAI |
| lightweight | small local model | Groq |
| default | configured primary | configured fallback |

## 4.7 SQLite Database Schema

```sql
-- tasks table
CREATE TABLE tasks (
    id TEXT PRIMARY KEY,
    action TEXT NOT NULL,
    target TEXT,
    parameters TEXT,          -- JSON
    execute_at INTEGER NOT NULL,
    repeat INTEGER DEFAULT 0,
    interval_seconds INTEGER DEFAULT 0,
    status TEXT DEFAULT 'pending',  -- pending|running|completed|cancelled|failed
    source TEXT,
    priority TEXT DEFAULT 'normal',
    created_at INTEGER,
    requires_approval INTEGER DEFAULT 0
);

-- permissions table
CREATE TABLE permissions (
    plugin TEXT NOT NULL,
    permission TEXT NOT NULL,
    allowed INTEGER DEFAULT 0,
    PRIMARY KEY (plugin, permission)
);

-- sessions table
CREATE TABLE sessions (
    session_id TEXT PRIMARY KEY,
    platform TEXT NOT NULL,
    chat_id TEXT,
    context TEXT,             -- JSON blob
    created_at INTEGER,
    last_activity INTEGER
);

-- logs table (future)
CREATE TABLE logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    level TEXT,
    message TEXT,
    timestamp INTEGER,
    source TEXT
);
```

---

# 5. PERMISSION SYSTEM

## Risk Levels

| Level | Behavior | Examples |
|-------|----------|----------|
| **LOW** | Auto-allowed | reminders, screenshots, notifications, open apps, volume, clipboard write, enumerate windows, get system stats |
| **MEDIUM** | Require confirmation | close processes, keyboard/mouse automation, scheduling tasks, config reload, clipboard read |
| **HIGH** | Always require approval | CMD execution, PowerShell, BAT scripts, registry edits, file delete, network modifications, plugin management |

## Execution Validation Chain

```
User Request
    ↓
AI Parsing
    ↓
Structured Action (JSON)
    ↓
Schema Validation
    ↓
Permission Check (risk level → auto/confirm/deny)
    ↓
Action Executor
    ↓
Result
```

---

# 6. AI WORKER PROTOCOL (Python ↔ Agent)

## Worker Lifecycle

1. Agent spawns: `python ai_workers/parser.py --input <temp_file> --output <temp_file>`
2. Agent writes prompt to input file
3. Python AI Worker reads input, calls AI provider, parses response
4. Python AI Worker writes structured JSON to output file
5. Python AI Worker exits
6. Agent reads output file

### Input Format (written by agent)

```json
{
  "user_message": "Close Discord after 50 minutes",
  "system_prompt": "You are SARA's command parser. Convert user request to structured action JSON.",
  "available_actions": ["close_process", "open_app", "take_screenshot", ...],
  "config": {
    "provider": "openai",
    "model": "gpt-4o-mini",
    "api_key": "...",
    "endpoint": "..."
  },
  "session_context": {}
}
```

### Output Format (read by agent)

```json
{
  "success": true,
  "structured_action": {
    "action": "close_process",
    "target": "Discord.exe",
    "parameters": {},
    "execute_at": 0,      // if delayed
    "repeat": false,
    "interval_seconds": 0
  },
  "response_text": "Okay, I will close Discord in 50 minutes.",
  "needs_clarification": false,
  "clarification_question": ""
}
```

---

# 7. TELEGRAM INTEGRATION

## Polling-Based Gateway

- **Polling interval**: 1-2 seconds (configurable)
- **Method**: `getUpdates` long polling
- **Flow**: Message → Parse → AI Route → Execute → Reply

### Supported Commands (hardcoded, no AI needed):

| Command | Action |
|---------|--------|
| `/start` | Welcome message |
| `/help` | Command list |
| `/status` | Runtime status |
| `/tasks` | List active tasks |
| `/cancel <id>` | Cancel task |
| `/screenshot` | Take screenshot |
| `/ping` | Latency check |

### Natural Language (requires AI):

Anything else → sent to AI Parser → structured action → execute.

---

# 8. BUILD SYSTEM SPECIFICATION

## C++ Build Configuration (CMake)

```cmake
cmake_minimum_required(VERSION 3.20)
project(sara_agent VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(SQLite3 REQUIRED)

# Sources (generated from src/*.cpp)
add_executable(sara_agent
    src/main.cpp
    src/Logger.cpp
    src/ConfigManager.cpp
    src/Scheduler.cpp
    src/SQLiteStore.cpp
    src/WinAPIExecutor.cpp
    src/TelegramGateway.cpp
    src/IPCServer.cpp
    src/AIWorkerLauncher.cpp
    src/ActionValidator.cpp
    src/PermissionManager.cpp
    src/SessionManager.cpp
)

target_include_directories(sara_agent PRIVATE include)
target_link_libraries(sara_agent PRIVATE sqlite3 ws2_32)
```

### Compiler Flags
- **MSVC (Windows)**: `/std:c++17 /O2 /MT /W4`
- **MinGW-w64**: `-std=c++17 -O2 -static -Wall -Wextra`
- **Link**: `-lws2_32 -lsqlite3 -luser32 -lgdi32 -lshell32`

---

# 9. IMPLEMENTATION ORDER (Phases & Steps)

## PHASE 1 — Core Foundation (Steps 1-10)
### Must be built FIRST in this order:

```
Step 1: Directory structure + CMakeLists.txt
Step 2: Logger (Logger.h + Logger.cpp)
Step 3: ConfigManager (ConfigManager.h + ConfigManager.cpp + configs/config.json)
Step 4: Scheduler core + main loop (Scheduler.h + Scheduler.cpp + main.cpp)
Step 5: SQLite persistence (SQLiteStore.h + SQLiteStore.cpp)
Step 6: WinAPIExecutor (WinAPIExecutor.h + WinAPIExecutor.cpp)
Step 7: TelegramGateway (TelegramGateway.h + TelegramGateway.cpp)
Step 8: IPCServer (IPCServer.h + IPCServer.cpp)
Step 9: AIWorkerLauncher (AIWorkerLauncher.h + AIWorkerLauncher.cpp)
Step 10: Integration test — all modules together
```

## PHASE 2 — AI System (Steps 11-15)

```
Step 11: Python AI Worker (ai_workers/parser.py)
Step 12: Universal AI Provider Layer (AIProvider.h + AIProvider.cpp)
Step 13: AI Router (AIRouter.h + AIRouter.cpp)
Step 14: Action Validator (ActionValidator.h + ActionValidator.cpp)
Step 15: Permission System (PermissionManager.h + PermissionManager.cpp)
```

## PHASE 3 — Automation (Steps 16-20)

```
Step 16: Screenshot system (ScreenshotCapture.h + .cpp)
Step 17: Keyboard automation (KeyboardAutomation.h + .cpp)
Step 18: Process monitoring (ProcessMonitor.h + .cpp)
Step 19: Notifications (NotificationManager.h + .cpp)
Step 20: Shell execution (ShellExecutor.h + .cpp)
```

## PHASE 4 — Ecosystem (Steps 21-23)

```
Step 21: NetSense integration
Step 22: System Monitor integration
Step 23: Monitoring rules engine
```

## PHASE 5 — Plugins (Steps 24-27)

```
Step 24: Plugin Loader (PluginLoader.h + .cpp)
Step 25: Plugin Manifest system
Step 26: Plugin Router
Step 27: MCP-style tool routing
```

## PHASE 6 — GUI (Steps 28-30)

```
Step 28: sara_gui.exe (framework TBD)
Step 29: Live monitoring UI
Step 30: Permission management UI
```

## PHASE 7 — Expansion (Steps 31-35)

```
Step 31: Discord integration
Step 32: WhatsApp integration (low priority)
Step 33: Web dashboard
Step 34: Mobile companion
Step 35: Distributed agents
```

---

# 10. CRITICAL RULES AI MUST FOLLOW

```
╔══════════════════════════════════════════════════════════════╗
║  1. AI NEVER directly executes commands — always validate   ║
║  2. Python NEVER stays running — fire and forget            ║
║  3. GUI NEVER owns runtime — agent is independent           ║
║  4. Never use Electron/Chromium — native only               ║
║  5. Max 500 lines per source file — split at 400            ║
║  6. Prefer WinAPI over libraries — native first             ║
║  7. Keep idle RAM extremely low — no always-running AI      ║
║  8. Use RAII/smart pointers — no manual memory leaks        ║
║  9. All actions use standard schema — no ad-hoc formats     ║
║ 10. Build incrementally — don't try everything at once      ║
╚══════════════════════════════════════════════════════════════╝
```

---

# 11. PERFORMANCE TARGETS

| Metric | Target |
|--------|--------|
| Idle RAM | < 10 MB (minimal process) |
| Idle CPU | ~0% (sleeping loop) |
| Thread count | < 10 (thread pool, not per-task) |
| Startup time | < 1 second |
| AI response latency | depends on provider |
| Screenshot memory | compressed, cleaned immediately |
| Python worker lifetime | < 5 seconds per invocation |

---

# 12. MCP / EXTERNAL AI TOOL EXPOSURE

For integrating with Claude Code or other external AI agents, expose SARA as tool endpoints:

## Tool Definitions (MCP-style)

```json
{
  "tools": [
    {
      "name": "sara_execute_action",
      "description": "Execute a WinAPI automation action",
      "input_schema": {
        "type": "object",
        "properties": {
          "action": { "type": "string", "enum": ["open_app", "close_process", "send_keys", "take_screenshot", "notify", "run_cmd", "run_powershell", "get_system_stats", "focus_window", "enum_windows"] },
          "target": { "type": "string" },
          "parameters": { "type": "object" }
        },
        "required": ["action"]
      }
    },
    {
      "name": "sara_schedule_task",
      "description": "Schedule a delayed or repeating task",
      "input_schema": {
        "type": "object",
        "properties": {
          "action": { "type": "string" },
          "target": { "type": "string" },
          "execute_at": { "type": "integer" },
          "repeat": { "type": "boolean" },
          "interval_seconds": { "type": "integer" }
        },
        "required": ["action", "execute_at"]
      }
    },
    {
      "name": "sara_list_tasks",
      "description": "List all scheduled tasks",
      "input_schema": { "type": "object", "properties": {} }
    },
    {
      "name": "sara_cancel_task",
      "description": "Cancel a scheduled task",
      "input_schema": {
        "type": "object",
        "properties": { "task_id": { "type": "string" } },
        "required": ["task_id"]
      }
    },
    {
      "name": "sara_get_system_stats",
      "description": "Get system performance stats (CPU, RAM, etc)",
      "input_schema": {
        "type": "object",
        "properties": {
          "type": { "type": "string", "enum": ["cpu", "ram", "disk", "gpu", "net", "all"] }
        }
      }
    },
    {
      "name": "sara_send_notification",
      "description": "Show a Windows toast notification",
      "input_schema": {
        "type": "object",
        "properties": {
          "title": { "type": "string" },
          "message": { "type": "string" }
        },
        "required": ["message"]
      }
    },
    {
      "name": "sara_ai_parse",
      "description": "Parse natural language into a structured action",
      "input_schema": {
        "type": "object",
        "properties": {
          "text": { "type": "string" },
          "provider": { "type": "string" }
        },
        "required": ["text"]
      }
    }
  ]
}
```

This tool definition can be used directly with MCP-compatible AI agents (Claude Code, etc.) to give them control over the SARA runtime.

---

# 13. QUICKSTART FOR AI DEVELOPERS

## Entry Point

1. **Start with Phase 1, Step 1**: Create directory structure
2. **First code**: `sara_agent/src/Logger.h` + `Logger.cpp` (simplest module)
3. **Test command**: `cmake --build build` after each module
4. **Integration test**: Run `sara_agent.exe` with Telegram token → send `/status`

## File Creation Order (Phase 1)

```
sara_agent/include/Logger.h
sara_agent/src/Logger.cpp
sara_agent/include/ConfigManager.h
sara_agent/src/ConfigManager.cpp
configs/config.json
sara_agent/include/Scheduler.h
sara_agent/src/Scheduler.cpp
sara_agent/src/main.cpp
sara_agent/include/SQLiteStore.h
sara_agent/src/SQLiteStore.cpp
sara_agent/include/WinAPIExecutor.h
sara_agent/src/WinAPIExecutor.cpp
sara_agent/include/TelegramGateway.h
sara_agent/src/TelegramGateway.cpp
sara_agent/include/IPCServer.h
sara_agent/src/IPCServer.cpp
sara_agent/include/AIWorkerLauncher.h
sara_agent/src/AIWorkerLauncher.cpp
CMakeLists.txt
```

## Key Patterns

- All classes follow RAII (Resource Acquisition Is Initialization)
- All structs use standard JSON serialization (nlohmann/json or manual)
- All IPC messages use the uniform JSON format defined in section 4.2
- All actions use the uniform action schema defined in section 4.1
- All events use the uniform event format defined in section 4.3
- Python AI workers are launched per-request and terminate after output

---

# 14. AI HELPER CONTEXT PROMPT

When giving this to Claude Code or any AI coding agent, use this as the context prompt:

```
You are implementing SARA (Smart Autonomous Runtime Assistant), a 
lightweight native Windows AI assistant. Read AI_HANDOFF.md for the 
complete architecture, all APIs, schemas, and implementation plan.

CRITICAL CONSTRAINTS:
- Language: C++17/20 (agent), Python (AI workers)
- Max 500 lines per source file
- No Electron, no Chromium, no always-running Python
- WinAPI-first architecture
- All actions follow the standard JSON action schema
- AI NEVER executes commands directly

Start with Phase 1, Step 1 and build incrementally.
Each module must compile independently before proceeding.
```

---

*This document serves as the complete reference for any AI agent to understand, implement, and extend the SARA project. All APIs, schemas, protocols, and architecture decisions are documented above.*
