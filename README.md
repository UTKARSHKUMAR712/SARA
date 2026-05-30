# SARA — System Runtime Autonomous Agent

**SARA** is a native Windows remote runtime and autonomous agent that provides full PC control via Telegram, a web-based terminal, desktop GUI, and extensible plugin system. Written in C++20, compiled with MinGW GCC.

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                        sara_agent.exe                            │
│                                                                  │
│  ┌──────────┐  ┌──────────────┐  ┌──────────────────────────┐   │
│  │ Telegram  │  │  IPC Server  │  │  WebSocket Server         │   │
│  │ Gateway   │  │  (Named Pipe)│  │  (Port 9080)              │   │
│  └─────┬─────┘  └──────┬───────┘  └──────────┬───────────────┘   │
│        │               │                      │                  │
│  ┌─────▼───────────────▼──────────────────────▼───────────────┐  │
│  │                   MessageHandlers                          │  │
│  │  (Telegram + IPC + WebSocket message dispatch)             │  │
│  └─────┬───────────────┬──────────────────────┬───────────────┘  │
│        │               │                      │                  │
│  ┌─────▼──────┐  ┌─────▼──────┐  ┌────────────▼──────────────┐  │
│  │ Action      │  │ CommandMap │  │ NativeCommandRouter        │  │
│  │ Dispatcher  │  │ (NLP)      │  │ (/cmd, /system, etc.)     │  │
│  └─────┬──────┘  └────────────┘  └────────────────────────────┘  │
│        │                                                         │
│  ┌─────▼──────────────────────────────────────────────────────┐  │
│  │                   WinAPIExecutor                           │  │
│  │  Process launch, keystrokes, clipboard, volume, etc.       │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐    │
│  │ Process   │ │ Event    │ │ Network  │ │ Hotkey            │    │
│  │ Monitor   │ │ Automatn │ │ Monitor  │ │ Manager           │    │
│  └──────────┘ └──────────┘ └──────────┘ └──────────────────┘    │
│                                                                  │
│  ┌────────────────┐   ┌────────────────────────────────────┐     │
│  │ Spotify Plugin │   │ MCP Adapter (JSON-RPC over stdio)  │     │
│  └────────────────┘   └────────────────────────────────────┘     │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │              Remote Terminal Runtime                       │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │  │
│  │  │TerminalHttp  │  │TerminalSessn │  │Cloudflared   │  │  │
│  │  │Server (9081) │  │Manager       │  │Manager       │  │  │
│  │  └──────┬───────┘  └──────┬───────┘  └──────────────┘  │  │
│  │         │                 │                             │  │
│  │  ┌──────▼─────────────────▼───────┐                    │  │
│  │  │       ConPTYSession           │                    │  │
│  │  │  (Windows Pseudo Console)     │                    │  │
│  │  └──────────────────────────────┘                    │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
          │                    │
  ┌───────▼────┐     ┌────────▼────────┐
  │ Browser    │     │ sara_gui.exe    │
  │ (xterm.js) │     │ (ImGui + D3D11) │
  │ HTTPS/WSS  │     │ Named Pipe IPC  │
  └────────────┘     └─────────────────┘
```

## Components

### sara_agent.exe (Core Agent)
The main executable that runs as Administrator. Connects to Telegram, executes commands, manages the terminal server, and coordinates all subsystems.

- **Telegram Gateway** — Polls Telegram Bot API for messages, sends replies with inline keyboards, docks, and media
- **Action Dispatcher** — Routes validated actions to appropriate handlers
- **CommandMap** — Natural language processing for commands like "play music", "open calculator"
- **WinAPI Executor** — Executes Windows API calls: launch processes, send keystrokes, control volume, clipboard ops, etc.
- **System Monitor** — CPU, RAM, GPU, battery, network stats
- **Process Monitor** — Tracks process start/stop events
- **Event Automation** — Rule-based automation engine
- **Scheduler** — Delayed and recurring task execution
- **Security Manager** — Rate limiting, trusted users, two-factor auth flow
- **Plugin Manager** — Dynamic DLL plugin system

### Remote Terminal Runtime
A full Windows terminal accessible via any browser through Cloudflare tunnel:

- **ConPTYSession** — Windows Pseudo Console (`CreatePseudoConsole`) wrapping real shell processes
- **TerminalHttpServer** — Embedded HTTP+WebSocket server serving an xterm.js frontend
- **TerminalSessionManager** — Session lifecycle (create, attach, detach, expiry, cleanup)
- **CloudflaredManager** — Manages `cloudflared tunnel --url` process for HTTPS access
- **Token Auth** — 64-char hex tokens from `BCryptGenRandom`

### sara_gui.exe (Desktop GUI)
A lightweight ImGui + DirectX 11 control panel for configuration and monitoring when remote access isn't needed.

### Spotify Plugin
Compiled-in plugin that connects to the Spotify Desktop app via its local WebSocket API for playback control, playlist management, and interactive Telegram docks.

### MCP Adapter
Model Context Protocol support — connects AI tool servers via JSON-RPC over stdio (e.g., DuckDuckGo search via `uvx`).

## Directory Structure

```
sara/
├── bot/                    # Start/restart/kill batch scripts
├── build/                  # CMake+Ninja build output
├── data/                   # Runtime data (SQLite DB, trusted users, screenshots)
├── docs/                   # Documentation (empty)
├── logs/                   # Runtime log files
├── plugins/
│   ├── plugin_api.h        # DLL plugin interface
│   └── spotify/            # Spotify plugin (compiled-in)
├── remote_runtime/         # Terminal subsystem
│   ├── include/            # Headers (ConPTY, HTTP server, sessions, tokens, cloudflare)
│   └── src/                # Implementation
├── runtime/                # Runtime directory (cloudflared.exe)
├── sara_agent/             # Main agent source
│   ├── include/            # 32 header files
│   └── src/                # 33 source files + MCP adapter
├── sara_gui/               # GUI control panel
├── settings/               # Config files (command_map.json, mcp_servers.json)
├── shared/                 # Third-party libs (imgui, json.hpp, sqlite3)
├── CMakeLists.txt          # Root build file
├── settings.json           # Global configuration
└── terminal.md             # Terminal subsystem architecture spec
```

## Build Instructions

### Prerequisites
- MSYS2 UCRT64 with MinGW GCC (C++20 support)
- CMake 3.20+
- Ninja build system

### Build
```bash
cd sara/build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

This produces:
- `sara/build/sara_agent.exe` — The main agent (requires Administrator)
- `sara/build/sara_gui.exe` — Desktop GUI control panel

### Quick Compile Check
```bash
compile_check.bat
```

## Configuration

### settings.json (root)
```json
{
  "telegram": {
    "token": "<bot_token>",
    "password": "<session_password>",
    "polling_interval_ms": 2000,
    "allowed_user_ids": []
  },
  "terminal_port": 9081,
  "terminal_shell": "powershell.exe",
  "terminal_expiry_minutes": 120,
  "cloudflare_enabled": true,
  "cloudflare_mode": "quick"
}
```

### Other Configuration
- `settings/command_map.json` — Natural language command definitions (700+ commands)
- `settings/mcp_servers.json` — MCP tool server definitions
- `settings/plugins.json` — Plugin configuration
- `settings/cmd_help.json` — Windows CLI help reference

## Security Model

1. **Two-Factor Auth** — Root password (shown on host) + session password from config
2. **Rate Limiting** — 5 messages/second per user
3. **Trusted Users** — Persistent whitelist in `data/trusted_users.json`
4. **Token Security** — `BCryptGenRandom` for terminal session tokens
5. **UAC** — Agent runs as Administrator (manifest embedded in exe)
6. **Session Expiry** — Terminals auto-expire (configurable, default 120 min)

## Usage

### Telegram Commands
| Command | Description |
|---------|-------------|
| `/terminal` | Create a new terminal session (get browser link) |
| `/terminal admin` | Create an elevated (Admin) terminal session |
| `/killterminal` | Destroy a terminal session |
| `/terminals` | List active terminal sessions |
| `/cmd <command>` | Run a Windows CLI command |
| `/status` | System status overview |
| `/monitor` | Real-time system resource monitor |
| `/screenshot` | Capture desktop screenshot |
| `/photo` | Capture webcam photo |
| `/tasks` | List scheduled tasks |
| `/rules` | List event automation rules |
| `/sararestart` | Restart the SARA agent |
| `/sarashutdown` | Shutdown SARA |

Natural language commands (via CommandMap): "play music", "open calculator", "search google for...", etc.

### Bot Scripts
```
bot\start_sara.bat      — Launch sara_agent.exe
bot\kill_sara.bat       — Kill cloudflared + sara_agent
bot\sara_restart.bat    — Restart both processes
```

## Mobile Terminal App

A React Native (Expo) companion app is available at `sara-terminal-app/` — provides a native mobile WebView terminal with tab support, theme selection, font sizing, and command bar. See its own README for details.

## Platform

- **Target:** Windows (10/11)
- **Compiler:** MinGW GCC (MSYS2 UCRT64)
- **Language:** C++20
- **Build:** CMake + Ninja
- **GUI:** DirectX 11 + ImGui
- **SQL:** SQLite (amalgamation)
