# SARA REMOTE RUNTIME INFRASTRUCTURE OVERHAUL

We are NO LONGER building:

* a chatbot,
* a fake web terminal,
* a command executor,
* a simulated shell.

We are upgrading SARA into:
a REAL native remote runtime infrastructure.

The target level is:

* AWS CloudShell
* Azure Cloud Shell
* GitHub Codespaces terminal
* VSCode Remote terminal
* Claude CLI environments
* Gemini CLI environments

BUT:

* self-hosted
* Windows-native
* ultra lightweight
* admin-capable
* browser-accessible
* persistent runtime

---

# VERY IMPORTANT ARCHITECTURE RULE

DO NOT modify existing stable runtime heavily.

Build the new remote runtime infrastructure inside:
a COMPLETELY SEPARATE FOLDER.

```
/remote_runtime
  /include
    ConPTYSession.h
    TerminalSessionManager.h
    TerminalHttpServer.h
    TerminalToken.h
  /src
    ConPTYSession.cpp
    TerminalSessionManager.cpp
    TerminalHttpServer.cpp
    TerminalToken.cpp
  /frontend
    index.html        ← xterm.js terminal page (served by HTTP server)
    terminal.js       ← WebSocket ↔ xterm.js bridge
    terminal.css      ← minimal dark terminal styling
```

This new infrastructure connects into sara_agent.exe through:

* TerminalHttpServer (port 9081) — NEW, separate from GUI WS (9080)
* TerminalSessionManager singleton — session lifecycle
* IPC bridge for Telegram trigger
* main.cpp startup hook (tiny addition)

The current runtime should remain stable while the new architecture is developed separately.

---

# FINAL ARCHITECTURE

```
Telegram
  ↓
/terminal command
  ↓
MessageHandlers.cpp (tiny addition)
  ↓
TerminalSessionManager::create_session()
  ↓
ConPTYSession::start()   ← CreatePseudoConsole() — REAL PTY
  ↓
generate token + session URL
  ↓
Telegram reply:
  "🖥 Terminal Ready
   Session: abc123
   Expires: 30 minutes
   Open: http://HOST:9081/t/abc123?token=xyz456"
  ↓
User opens browser link
  ↓
TerminalHttpServer serves index.html
  ↓
xterm.js opens WebSocket: ws://HOST:9081/ws/abc123?token=xyz456
  ↓
TerminalSessionManager validates token → routes WS to ConPTYSession
  ↓
ConPTYSession pipes bytes: browser ↔ real local PowerShell/cmd
```

PORT MAP:
- 9080 — existing GUI/plugin WebSocket (DO NOT TOUCH)
- 9081 — new terminal HTTP + WS server (NEW)

Telegram is ONLY: session launcher.
Telegram is NOT: terminal controller.

After browser opens:
ALL interaction happens directly between browser ↔ sara_agent.exe (port 9081).

---

# WEBSOCKET TERMINAL PROTOCOL

Binary-safe base64 encoding for all terminal data:

```
Browser → Server:
  { "type": "input",  "data": "<base64 encoded bytes>" }
  { "type": "resize", "cols": 220, "rows": 50 }
  { "type": "ping" }

Server → Browser:
  { "type": "output", "data": "<base64 encoded bytes>" }
  { "type": "pong" }
  { "type": "exit",   "code": 0 }
```

Data is base64-encoded to safely carry binary PTY output:
ANSI escape codes, UTF-8, control characters, progress bar rewrites.

---

# CRITICAL REQUIREMENT — REAL TERMINAL

The browser terminal MUST behave like:
a REAL native terminal.

NOT:
a fake command executor.

NOT:
API command execution.

NOT:
simulated shell behavior.

The browser terminal should feel nearly identical to:

* Windows Terminal
* PowerShell
* VSCode terminal
* SSH session
* CloudShell terminal

---

# REQUIRED EXECUTION MODEL

If user types:

```
cd project
claude
```

inside browser terminal,

then:
the REAL locally installed Claude CLI must launch normally.

If user types:

```
gemini
```

then:
REAL Gemini CLI launches locally.

This system must support:
ALL normal CLI workflows.

The browser is ONLY:
a rendering + input layer.

The REAL shell runs locally on the machine.

---

# REQUIRED TERMINAL PARITY

The terminal must support:

* claude cli
* gemini cli
* npm
* pnpm
* node
* python
* pip
* cargo
* git
* ssh
* REPLs
* interactive applications
* long-running processes
* ANSI colors
* UTF-8
* live redraws
* progress bars
* interactive prompts
* shell history
* ctrl+c  (forwarded as raw \x03 — no interception)
* tab completion
* arrow keys
* multiline input

All of the above work natively through ConPTY passthrough.
xterm.js handles rendering. No simulation required.

---

# CRITICAL SESSION REQUIREMENT

ONE browser session = ONE persistent ConPTY shell session.

Meaning:

* shell state persists
* working directory persists
* environment variables persist
* running processes persist between WS reconnects

DO NOT spawn separate process per command.

This is REQUIRED for real terminal parity.

---

# REQUIRED WINDOWS TERMINAL IMPLEMENTATION

Use Windows ConPTY API properly.

```cpp
// Core ConPTY pattern:
CreatePipe(&pipe_in_read,  &pipe_in_write,  nullptr, 0);
CreatePipe(&pipe_out_read, &pipe_out_write, nullptr, 0);

CreatePseudoConsole({cols, rows}, pipe_in_read, pipe_out_write, 0, &hpc_);

// STARTUPINFOEXW with PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE attribute
CreateProcessW(shell_cmd, nullptr, nullptr, nullptr, FALSE,
    EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
    nullptr, nullptr, &si.StartupInfo, &pi_);

// Read loop (background thread):
ReadFile(pipe_out_read, buf, sizeof(buf), &bytes_read, nullptr);
// → base64 encode → send as { type:"output", data:"..." } via WS

// Write (from browser input):
// → base64 decode → WriteFile(pipe_in_write, ...)

// Resize:
ResizePseudoConsole(hpc_, {cols, rows});
```

DO NOT:
* fake terminal behavior
* simulate command execution
* use command wrappers
* use hidden cmd hacks
* create fake stdin-only pipes

---

# REQUIRED TOKEN SECURITY

```cpp
// Token generation — cryptographically secure:
BYTE random_bytes[32];
BCryptGenRandom(nullptr, random_bytes, 32, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
// hex encode → 64-char token string

// Session ID — 8 alphanumeric chars from random bytes
```

Token validation:
- Checked on HTTP page serve
- Checked on WebSocket upgrade
- Checked on every reconnect attempt
- Bound to owner_chat_id (only one Telegram user can use it)

Session expires: 30 minutes after last PTY activity (configurable in settings.json).

---

# REQUIRED FRONTEND

Frontend stack:

```
xterm.js only (loaded from CDN — zero build step)
```

Minimal:
* HTML
* CSS
* JS

NO:
* React / Vue / Angular
* Electron
* Node.js server

```html
<!-- index.html core: -->
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/xterm/css/xterm.css"/>
<script src="https://cdn.jsdelivr.net/npm/xterm/lib/xterm.js"></script>
<script src="https://cdn.jsdelivr.net/npm/xterm-addon-fit/lib/xterm-addon-fit.js"></script>
<script src="https://cdn.jsdelivr.net/npm/xterm-addon-web-links/lib/xterm-addon-web-links.js"></script>
<script src="/static/terminal.js"></script>
```

Browser handles: ONLY terminal rendering + WS I/O.

---

# REQUIRED BACKEND

Build ONE shared:
* HTTP server (serves frontend HTML/JS/CSS)
* WebSocket server (streams PTY I/O)
* event loop

inside sara_agent.exe as `TerminalHttpServer` on port 9081.

All terminal sessions share same runtime infrastructure.
DO NOT create separate webserver per session.

---

# REQUIRED TERMINAL SESSION MANAGER

```
TerminalSessionManager (singleton)
```

Responsibilities:

* create_session(chat_id, cols, rows) → {session_id, token, url}
* destroy_session(session_id)
* get_session(session_id, token) → validates token, returns session ptr
* attach_websocket(session_id, token, socket)
* cleanup_expired() — background thread, every 60 seconds
* shutdown_all()

TerminalSession struct:
* id: 8-char alphanumeric
* token: 64-char hex
* owner_chat_id: string (Telegram chat ID)
* pty: ConPTYSession
* ws_socket: int (current attached WS connection, or -1)
* created_at, last_active, expires_at: timestamps

---

# RECONNECT SUPPORT

If user refreshes browser or network drops:
1. WS reconnects with same session_id?token=TOKEN
2. TerminalSessionManager validates token → re-attaches WS to same PTY
3. PTY process was still running — state preserved
4. xterm.js shows continuation of shell (no restart)

If PTY process exits naturally:
- Server sends: { "type": "exit", "code": 0 }
- xterm.js shows: "\r\n[Session ended — process exited with code 0]\r\n"

Browser reconnect JS:
- If WS closes: retry 3x with 2s backoff
- After 3 failures: show "Session ended or expired" message in terminal

---

# ADMINISTRATOR SUPPORT

sara_agent.exe must support running as Administrator.

If elevated:
browser terminal sessions inherit:
FULL administrator shell capabilities.

Examples:
* admin PowerShell
* registry access
* service management
* system commands
* elevated scripts
* WinAPI automation

The browser terminal behaves like a REAL elevated terminal.

---

# REQUIRED TELEGRAM FLOW

Telegram ONLY:
* sends /terminal command
* receives URL reply
* opens browser link

Example bot reply:
```
🖥 Terminal Ready

Session: abc123
Expires: 30 minutes

Open:
http://192.168.1.100:9081/t/abc123?token=a1b2c3...64chars
```

After that: Telegram is OUT of the runtime loop.

Additional Telegram commands:
* /killterminal — destroys current session immediately
* /terminals — lists active sessions (Phase 5)

---

# CONFIGURATION

Add to settings.json:
```json
{
  "terminal_host": "192.168.1.100",
  "terminal_port": 9081,
  "terminal_shell": "powershell.exe",
  "terminal_expiry_minutes": 30,
  "terminal_default_cols": 220,
  "terminal_default_rows": 50
}
```

Add to AppConfig struct in ConfigManager.h:
```cpp
std::string terminal_host = "localhost";
int         terminal_port = 9081;
std::string terminal_shell = "powershell.exe";
int         terminal_expiry_minutes = 30;
int         terminal_default_cols = 220;
int         terminal_default_rows = 50;
```

---

# SECURITY REQUIREMENTS

CRITICAL. This system exposes REAL terminal access.

Implement:
* BCryptGenRandom 32-byte tokens
* expiration (30 min inactivity)
* WebSocket auth (token in query param, validated on upgrade)
* isolated sessions (each session has own ConPTY process)
* ownership validation (only owner_chat_id can use session)
* cleanup (background thread kills expired sessions)
* session revocation (/killterminal command)
* reconnect validation (token required every reconnect)

Future-ready:
* HTTPS (add TLS layer: SChannel or mbedTLS)
* IP allowlist (check client IP on WS connect)
* permission levels (read-only vs read-write sessions)

---

# PERFORMANCE REQUIREMENTS

The runtime MUST remain:
* ultra lightweight
* low RAM
* low CPU
* async
* event-driven

Target: near 0% CPU idle, minimal RAM per session.

DO NOT:
* embed Chromium
* use Electron
* create polling loops (use blocking ReadFile with dedicated read thread)
* create heavyweight frontend
* use process-per-command execution

Each ConPTY session: one background reader thread + one PTY process.
No polling loops — ReadFile blocks until data available.
WS write is called only when PTY produces output.

---

# IMPLEMENTATION ORDER

## PHASE 1 — Core PTY + Session Manager
* ConPTYSession.h / .cpp  (CreatePseudoConsole)
* TerminalToken.h / .cpp  (BCryptGenRandom)
* TerminalSessionManager.h / .cpp
* CMakeLists.txt update (add remote_runtime sources)
* Compile + PTY smoke test (headless: create session, run dir, read output)

## PHASE 2 — Browser Frontend + HTTP Server
* TerminalHttpServer.h / .cpp
* frontend/index.html
* frontend/terminal.js
* frontend/terminal.css
* End-to-end browser test

## PHASE 3 — Telegram Integration
* MessageHandlers.cpp: add /terminal handler
* ConfigManager.h: add terminal config fields
* main.cpp: start TerminalHttpServer on startup
* command_map.json: add "open terminal" natural language phrase
* Full Telegram → browser flow test

## PHASE 4 — Hardening
* Token expiry + cleanup thread
* Browser reconnect logic (3x retry with 2s backoff)
* /killterminal command
* Exit code detection → notify browser

## PHASE 5 — Multi-session + Expansion
* Multi-session routing
* /terminals list command
* Session dashboard (future)
* Browser file manager (future)
* IDE/runtime expansion (future)

---

# LONG TERM GOAL

This remote runtime infrastructure should later evolve into:

* remote development environment
* browser IDE
* runtime portal
* automation console
* monitoring console
* MCP execution runtime
* autonomous execution environment
* VSCode-style remote workflows

BUT initial implementation focuses ONLY on:
* REAL PTY passthrough
* WebSocket streaming
* lightweight architecture
* stable session management
* native terminal parity

---

# FINAL IMPORTANT RULE

Build this as:
REAL terminal remoting infrastructure.

NOT:
fake command execution system.

The REAL local shell must execute everything naturally.
The browser is simply a transparent viewport into the local machine.
