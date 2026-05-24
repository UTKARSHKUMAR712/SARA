# SARA Project Status

## Project Structure

```
C:\Users\utkarsh_kumar\Desktop\sara\
|
|-- CMakeLists.txt                          # Build system (CMake 3.20+, C++17)
|-- settings.json                           # Runtime config (Telegram, AI provider)
|-- readme.md                               # Project overview
|-- AI_HANDOFF.md                           # AI agent reference docs
|-- implemention.md                         # Implementation plan (7 phases)
|-- importantdetial.md                      # Engineering specs
|-- focus.md                                # Risk analysis
|
|-- ai_workers/
|   |-- parser.py                           # Python AI worker (15 tools, OpenAI-compatible)
|
|-- sara_agent/                             # Core runtime
|   |-- include/                            # Headers (14 files)
|   |-- src/                                # Sources (15 files + sqlite3.c)
|
|-- sara_gui/                               # GUI control panel (Dear ImGui + D3D11)
|   |-- include/
|   |   |-- IPCClient.h
|   |-- src/
|       |-- main.cpp
|       |-- IPCClient.cpp
|
|-- shared/                                 # Third-party libs
|   |-- sqlite3.c / sqlite3.h
|   |-- nlohmann/json.hpp
|   |-- imgui/ (Dear ImGui v1.91, Win32 + D3D11 backends)
|
|-- build/                                  # Build output (ninja-based)
|   |-- sara_agent.exe
|   |-- sara_gui.exe
|
|-- data/
|   |-- scheduler.db                        # SQLite DB (tasks, permissions, sessions, memory)
|   |-- screenshots/                        # Captured screenshots
|
|-- logs/
|   |-- sara_YYYYMMDD.log
|
|-- plugins/                                # Empty (plugin system not implemented)
|-- docs/                                   # Empty
```

## Component Status

### CORE RUNTIME (`sara_agent.exe`)

| Component | Status | Lines | What It Does |
|-----------|--------|-------|-------------|
| **main.cpp** | ✅ Complete | 473 | Entry point, message routing, Telegram command handlers |
| **Logger** | ✅ Complete | 133 | Thread-safe file logger, 4 levels, daily log files |
| **ConfigManager** | ✅ Complete | 174 | JSON config load/save, AI/Telegram/App config structs |
| **Scheduler** | ⚠️ Bug | 258 | Background task loop, pending/completed/cancelled status |
| **SQLiteStore** | ✅ Complete | 299 | 4 tables (tasks, permissions, sessions, memory), prepared stmts |
| **WinAPIExecutor** | ✅ Complete | 504 | 17 actions: open_app, close_process, send_keys, run_cmd, etc. |
| **TelegramGateway** | ⚠️ Bug | 358 | Long-polling bot, send/recv messages + photos |
| **IPCServer** | ✅ Complete | 192 | Named pipe server, JSON messages, broadcast |
| **AIWorkerLauncher** | ✅ Complete | 197 | Spawns Python worker per request, file-based protocol |
| **ActionValidator** | ✅ Complete | 145 | 24-action whitelist, LOW/MEDIUM/HIGH risk levels |
| **PermissionManager** | ⚠️ Partial | 122 | Framework exists but auto-approves everything |
| **SessionManager** | ⚠️ Partial | 164 | Tracks sessions but cleanup_expired() never called |
| **SystemMonitor** | ✅ Complete | 155 | CPU (PDH), RAM, disk, battery, process count |
| **ScreenshotCapture** | ✅ Complete | 189 | GDI BitBlt, fullscreen + window capture, BMP output |
| **CameraCapture** | ✅ Complete | 265 | Media Foundation, NV12/YUY2/RGB, BMP output |

### GUI (`sara_gui.exe`)

| Component | Status | Lines | What It Does |
|-----------|--------|-------|-------------|
| **main.cpp** | ✅ Complete | 387 | Dear ImGui + D3D11, config editor, log viewer |
| **IPCClient** | ⚠️ Inefficient | 106 | Pipe client, reconnects every message |

### AI WORKER

| Component | Status | Lines | What It Does |
|-----------|--------|-------|-------------|
| **parser.py** | ✅ Complete | 331 | 15 tools, OpenAI/Groq/Ollama, tool_calls + legacy function_call |

### DOCUMENTATION

| File | Lines | Purpose |
|------|-------|---------|
| readme.md | 702 | Overview, architecture, usage |
| AI_HANDOFF.md | 762 | API reference for AI agents |
| focus.md | 825 | Risk analysis (20 danger areas) |
| importantdetial.md | 711 | Engineering spec, IPC, schema, security |
| implemention.md | 670 | 7-phase / 35-step implementation plan |

---

## Feature Implementation Detail

### ✅ Fully Implemented (Working)

- Telegram message polling & response
- `/start`, `/help`, `/status`, `/ping`, `/tasks`, `/screenshot`, `/test`, `/show_config`, `/set_ai` commands
- Open apps (notepad, msedge, chrome, calc, etc.)
- Close processes by exe name
- Fullscreen & window screenshots (GDI)
- Camera capture (Media Foundation)
- CMD execution with stdout capture
- PowerShell execution (base64 UTF-16 encoding)
- System stats (CPU, RAM, disk, battery, process count)
- IP address retrieval (GetAdaptersAddresses)
- Clipboard read/write
- Volume set/mute
- Send keys (keyboard simulation via SendInput)
- Focus window by title
- Windows notifications (balloon)
- Task scheduling (one-shot + repeating)
- SQLite persistence (tasks, permissions, sessions, memory)
- AI natural language parsing (tool_calls)
- AI multi-action (multiple tool_calls in one response)
- Memory storage (remember user info)
- GUI configuration editor (AI provider, Telegram token)
- GUI live log viewer with auto-scroll
- GUI auto-launch agent & test AI
- IPC communication between agent and GUI

### ⚠️ Partially Implemented (Has Issues)

| Feature | Issue |
|---------|-------|
| **Permission system** | Framework exists but approval callback is hardcoded to auto-approve ALL actions from Telegram/GUI. No actual user confirmation flow. |
| **Action validation** | Whitelist is hardcoded, not config-driven. `capture_camera` and `get_ip_address` were missing initially. |
| **Session memory** | Sessions tracked but context passed to AI is unreliable. `cleanup_expired()` never called. Not persisted to SQLite. |
| **Scheduler repeat tasks** | Repeating logic exists but persistence is buggy — uses `add()` instead of `update()`, overwrites original task entry. |
| **Telegram photo upload** | Sends BMP (uncompressed, ~5MB). No image compression. |
| **AI provider flexibility** | Supports OpenAI, Groq, Ollama. Claude/Gemini/NVIDIA NIM not implemented on Python side. |

### ❌ Not Implemented (Missing)

| Feature | Phase | Notes |
|---------|-------|-------|
| **Mouse automation** | Phase 3 | `mouse_move`/`mouse_click` defined in schema but NOT in WinAPIExecutor |
| **Discord integration** | Phase 7 | No code |
| **WhatsApp integration** | Phase 7 | No code |
| **Plugin system** | Phase 5 | `plugins/` empty, no PluginLoader, no manifest system |
| **Web dashboard** | Phase 7 | Not started |
| **Mobile companion** | Phase 7 | Not started |
| **Distributed agents** | Phase 7 | Not started |
| **Log rotation** | Logger | Log file grows unbounded |
| **Toast notifications** | Phase 3 | Uses legacy balloon, not modern Windows Toast API |
| **Config encryption** | Security | API keys in plaintext in settings.json |
| **GUI task visualization** | Phase 6 | No task list display |
| **Permission UI** | Phase 6 | No confirmation dialog |
| **Screenshot compression** | Phase 3 | BMP only, no PNG/JPEG |
| **NetSense integration** | Phase 4 | Not started |
| **Monitoring rules engine** | Phase 4 | Not started |

---

## Bugs & Issues

### CRITICAL

| # | Issue | Location | Details |
|---|-------|----------|---------|
| 1 | **Telegram crash on null chat_id** | `TelegramGateway.cpp` | `msg["chat"]["id"]` throws when `chat` is null. Seen repeatedly in logs. Some Telegram updates (edited messages, service messages) lack a `chat` field. |
| 2 | **Hardcoded auto-approval bypasses security** | `main.cpp:449-452` | All actions auto-approved for Telegram/GUI, including HIGH risk (cmd, powershell, bat). Violates core security model. |
| 3 | **AI model config mismatch** | `settings.json` + `parser.py` | Model name `openai/gpt-oss-120b` on Groq endpoint may not exist or produce errors. Previous model `llama-3.3-70b-versatile` returned XML function format instead of JSON tool_calls. |
| 4 | **Repeat task persistence broken** | `Scheduler.cpp:176` | Uses `store_->add()` instead of `store_->update()` for repeating tasks. Original task status never updated to completed in DB. |

### MODERATE

| # | Issue | Location | Details |
|---|-------|----------|---------|
| 5 | **IPCClient reconnects per message** | `IPCClient.cpp:54` | Disconnect → reconnect on every `send_command`. Creates new named pipe per call. Race condition if called from multiple threads. |
| 6 | **GUI IPC poll misses updates** | `GUI main.cpp` | Naive `last_update_id` comparison may miss messages. Busy-wait sleep loop instead of event-driven. |
| 7 | **WinHTTP handles not reused** | `TelegramGateway.cpp` | New session/connect/request per API call. Works but wasteful for 2-second polling. |
| 8 | **send_keys truncated at ~256 chars** | `WinAPIExecutor.cpp:214` | Fixed 512-element `inputs` array. Beyond ~256 characters, remaining keys silently dropped. |
| 9 | **Notification blocks executor thread** | `WinAPIExecutor.cpp:370` | `sleep_for(10s)` blocks the calling thread. Could delay scheduler. |
| 10 | **Camera timeout hardcoded** | `CameraCapture.cpp:90` | 6 second timeout (60 × 100ms), not configurable, no cancel. |
| 11 | **Session ID generator duplicated** | `Scheduler.cpp` + `SessionManager.cpp` | Nearly identical `generate_uuid` functions. Should be centralized. |
| 12 | **get_system_stats ignores `type` param** | `WinAPIExecutor.cpp` | Schema defines `type: "cpu\|ram\|disk\|gpu\|net"` but always returns all. |

### MINOR

| # | Issue | Location | Details |
|---|-------|----------|---------|
| 13 | `enum_processes` buffer hardcoded at 1024 | `WinAPIExecutor.cpp:142` | May overflow with >1024 processes |
| 14 | Logger enum is `ERR` not `ERROR` | `Logger.h:13` | Inconsistent naming |
| 15 | `chat_id` serialized as string for integer | `TelegramGateway.cpp` | Works but sematically wrong |
| 16 | IPC server has no message format validation | `IPCServer.cpp` | Raw text starting with `/` causes parse error |
| 17 | No input sanitization in `run_cmd`/`run_powershell` | `WinAPIExecutor.cpp` | No validation + auto-approval = security hole |
| 18 | `capture_camera` and `get_ip_address` were missing from whitelist initially | `ActionValidator.cpp` | Had to be added manually |

---

## What Needs Fixing Next (Priority Order)

### P0 — Must Fix (Crashes / Security)

1. **Fix Telegram null chat_id crash** — Add null checks around `msg["chat"]`, `msg["from"]` before accessing their fields
2. **Fix security auto-approval** — At minimum, require explicit approval for HIGH risk actions. Add `allowed_user_ids` filtering in Telegram.
3. **Fix AI model config** — Clean up settings.json with a model name that actually works with Groq's API. Remove `openai/gpt-oss-120b` nonsense.

### P1 — Reliability

4. **Fix repeat task persistence** — Change `store_->add()` to `store_->update()` for repeating tasks
5. **Fix IPCClient efficiency** — Keep connection alive, don't reconnect per message. Add mutex for thread safety.
6. **Fix WinHTTP handle pooling** — Reuse session/connect handles across Telegram poll requests
7. **Fix send_keys truncation** — Warn or dynamically size the inputs array
8. **Fix notification blocking** — Move to async or reduce sleep time

### P2 — Features / Completeness

9. **Add mouse_move/mouse_click** to WinAPIExecutor
10. **Add screenshot compression** (PNG or JPEG output option)
11. **Add log rotation** — Archive log files by size or date
12. **Add config encryption** — At minimum obfuscate API keys in settings.json
13. **Implement GUI task list** display
14. **Implement permission confirmation** via Telegram inline buttons

### P3 — Nice to Have

15. Add `cleanup_expired()` calls to SessionManager
16. Centralize UUID generation into a shared utility
17. Add IPC message format validation
18. Add command sanitization for `run_cmd`/`run_powershell`
19. Make camera timeout configurable
20. Add rate limiting for Telegram commands

---

## Architecture Summary

```
Telegram (polling)           GUI (IPC client)
       |                          |
       v                          v
  [ TelegramGateway ]     [ IPCServer ] <--> [ IPCClient ]
       |                          |
       v                          v
  [ main.cpp (dispatcher) ]    [ sara_gui.exe ]
       |
       v
  [ ActionValidator ] --> [ PermissionManager ] --> [ WinAPIExecutor ]
       |                                                  |
       v                                                  v
  [ AIWorkerLauncher ] --> [ parser.py (Python) ]    [ SystemMonitor ]
       |                                               [ ScreenshotCapture ]
       v                                               [ CameraCapture ]
  [ Scheduler ] --> [ SQLiteStore ]
  [ SessionManager ]
```

### Key Design Properties

- **Two processes**: `sara_agent.exe` (daemon) + `sara_gui.exe` (control panel)
- **IPC**: Windows Named Pipes, JSON messages
- **AI**: Temporary Python subprocess per request (no persistent runtime)
- **Storage**: SQLite via bundled `sqlite3.c`
- **Security Chain**: AI → ActionValidator → PermissionManager → WinAPIExecutor
- **Build**: MSYS2 UCRT64, CMake + Ninja, C++17
  
  update2.0

