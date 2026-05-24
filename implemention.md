# SARA IMPLEMENTATION PLAN
## a single file should have maximum to maximum 500 lines of code not more than that if you see it exceeed 400 lines create new file and continue 
# Goal

Build SARA as:
- a lightweight native Windows assistant,
- an AI-assisted automation runtime,
- a modular ecosystem,
- a remotely controllable background agent,
- an event-driven WinAPI automation system.

Main priorities:
1. Lightweight runtime
2. Stable architecture
3. Native WinAPI integration
4. Remote control
5. Modular ecosystem
6. AI routing
7. Plugin support

---

# DEVELOPMENT PRINCIPLES

## MUST FOLLOW

- Keep RAM usage low
- Avoid unnecessary frameworks
- Avoid Electron
- Avoid Chromium
- Avoid always-running Python
- Avoid always-running LLMs
- Use WinAPI whenever possible
- Keep architecture modular internally
- Keep GUI independent from runtime

---

# MAIN COMPONENTS

## Core Runtime
Executable:
```text
sara_agent.exe
```

Language:
- C++17/20

Responsibilities:
- scheduler,
- automation,
- WinAPI execution,
- Telegram gateway,
- task persistence,
- AI routing,
- plugin loading,
- IPC server.

---

## GUI Controller
Executable:
```text
sara_gui.exe
```

Purpose:
- setup,
- settings,
- configuration,
- integrations,
- plugin management,
- logs.

Important:
- GUI must NOT own runtime
- GUI is optional
- Agent continues without GUI

---

# PHASE 1 — CORE FOUNDATION

# Objective

Create minimal working assistant runtime.

---

# STEP 1 — Project Structure

Create base folders.

```text
SARA/
│
├── sara_agent/
├── sara_gui/
├── shared/
├── plugins/
├── ai_workers/
├── configs/
├── logs/
├── data/
└── docs/
```

---

# STEP 2 — Create sara_agent.exe

Language:
- C++17/20

Requirements:
- single lightweight process
- background runtime
- low RAM usage
- no GUI dependency

Initial modules:
- Logger
- ConfigManager
- Scheduler
- IPCServer
- TelegramGateway
- WinAPIExecutor
- AIWorkerLauncher

---

# STEP 3 — Build Logger

Features:
- lightweight logging
- file logging
- timestamps
- log rotation later

Log levels:
- INFO
- WARNING
- ERROR
- DEBUG

---

# STEP 4 — Build Config System

Config format:
- JSON

Store:
- API keys
- endpoints
- Telegram token
- permissions
- model settings

Files:
```text
configs/config.json
```

---

# STEP 5 — Build Scheduler Core

Core responsibility:
- delayed tasks
- repeating tasks
- conditional tasks

Task model:

```json
{
  "id": 1,
  "action": "close_process",
  "target": "Discord.exe",
  "execute_at": 123456789
}
```

Scheduler loop:
```cpp
while(running)
{
    check_tasks();
    sleep_for(500ms);
}
```

Requirements:
- ultra-lightweight
- near-zero idle CPU

---

# STEP 6 — Build Task Persistence

Use:
- SQLite

Store:
- scheduled tasks
- repeating tasks
- history later

Initial tables:
- tasks
- logs later

---

# STEP 7 — Build WinAPI Executor

Responsibilities:
- open applications
- close applications
- keyboard automation
- screenshots
- notifications
- CMD execution
- PowerShell execution

Initial APIs:
- CreateProcess
- SendInput
- ShellExecute
- TerminateProcess
- EnumWindows

---

# STEP 8 — Build Telegram Gateway

FIRST remote platform.

Reason:
- easiest
- lightweight
- HTTP-based

Features:
- polling messages
- sending replies
- command forwarding

Flow:
Telegram
    ↓
Gateway
    ↓
AI Parser
    ↓
Scheduler/Executor

---

# STEP 9 — Build IPC Server

Purpose:
- communication with GUI
- future plugins
- external integrations

Preferred:
- Named Pipes

Alternative:
- localhost sockets

---

# STEP 10 — Build AI Worker Launcher

Responsibilities:
- start temporary Python workers
- pass prompts
- receive structured JSON
- terminate worker

IMPORTANT:
- Python should NOT remain running permanently

---

# PHASE 2 — AI SYSTEM

# Objective

Add natural language understanding.

---

# STEP 11 — Create Python AI Worker

Location:
```text
ai_workers/parser.py
```

Responsibilities:
- command parsing
- reasoning
- structured task generation

Input:
```text
"Close Discord after 50 minutes"
```

Output:
```json
{
  "action": "close_process",
  "target": "Discord.exe",
  "delay_seconds": 3000
}
```

---

# STEP 12 — Build Universal AI Provider Layer

Support:
- OpenAI
- Claude
- Gemini
- Groq
- NVIDIA NIM
- Ollama
- custom APIs

Universal config:

```json
{
  "provider": "openai_compatible",
  "endpoint": "",
  "api_key": "",
  "model": ""
}
```

---

# STEP 13 — Build AI Router

Responsibilities:
- choose provider
- fallback handling
- normalize APIs
- route requests

Examples:
- coding → Claude
- fast → Groq
- local → Ollama

---

# STEP 14 — Build Structured Action Validator

Purpose:
- prevent unsafe execution
- validate AI outputs

Checks:
- allowed commands
- permission level
- malformed actions

---

# STEP 15 — Build Permission System

Levels:
- low risk
- medium risk
- high risk

Examples:
- screenshots → low
- closing apps → medium
- CMD → high

---

# PHASE 3 — AUTOMATION SYSTEM

# Objective

Expand automation capabilities.

---

# STEP 16 — Add Screenshot System

Methods:
- BitBlt
- clipboard capture
- PrintScreen simulation

Features:
- delayed screenshots
- repeating screenshots

---

# STEP 17 — Add Keyboard Automation

Using:
- SendInput

Features:
- shortcuts
- macros
- automation sequences

---

# STEP 18 — Add Process Automation

Features:
- process monitoring
- auto close
- auto restart

---

# STEP 19 — Add Notifications

Using:
- Windows Toast Notifications

Features:
- reminders
- alerts
- warnings

---

# STEP 20 — Add Shell Execution

Features:
- CMD
- PowerShell
- temporary BAT scripts

Requirements:
- permission approval
- logging

---

# PHASE 4 — ECOSYSTEM INTEGRATION

# Objective

Connect external ecosystem tools.

---

# STEP 21 — Integrate NetSense

Capabilities:
- network stats
- bandwidth
- latency
- diagnostics

Use cases:
"Why is my internet slow?"

---

# STEP 22 — Integrate System Monitor

Capabilities:
- CPU usage
- RAM usage
- GPU stats
- temperatures

Use cases:
"Why is my PC lagging?"

---

# STEP 23 — Add Monitoring Rules

Examples:
- CPU > 90%
- RAM > 95%
- ping spikes

Actions:
- notify
- log
- automate

---

# PHASE 5 — PLUGIN SYSTEM

# Objective

Add modular ecosystem support.

---

# STEP 24 — Create Plugin Loader

Plugin types:
- executable plugins
- API plugins
- Python connectors

Avoid:
- unsafe DLL injection initially

---

# STEP 25 — Create Plugin Manifest Format

Example:
```json
{
  "name": "netsense",
  "version": "1.0",
  "commands": [
    "network_stats"
  ]
}
```

---

# STEP 26 — Add Plugin Routing

Flow:
Command
    ↓
AI Router
    ↓
Plugin Selection
    ↓
Execution

---

# STEP 27 — Add MCP-style Tool Routing

Examples:
- Claude Code
- external agents
- coding tools
- automation tools

---

# PHASE 6 — GUI

# Objective

Create optional control center.

---

# STEP 28 — Build sara_gui.exe

Features:
- settings
- logs
- task list
- plugin management
- API management
- remote setup

IMPORTANT:
- GUI must remain independent

---

# STEP 29 — Add Live Monitoring UI

Show:
- tasks
- CPU
- memory
- plugins
- network stats

---

# STEP 30 — Add Permission UI

Allow:
- approve commands
- revoke permissions
- manage automation

---

# PHASE 7 — EXPANSION

# Objective

Add advanced ecosystem features.

---

# STEP 31 — Add Discord Integration

Features:
- command handling
- responses
- remote control

---

# STEP 32 — Add WhatsApp Integration

Future support.

---

# STEP 33 — Add Web Dashboard

Optional remote management.

---

# STEP 34 — Add Mobile Companion

Optional future feature.

---

# STEP 35 — Add Distributed Agent Support

Future:
- multiple nodes
- remote systems
- shared automation

---

# FINAL TARGET

SARA should become:
- ultra-lightweight
- native Windows assistant
- AI orchestration layer
- WinAPI automation runtime
- remotely controllable
- plugin-driven
- ecosystem-based
- modular
- stable

SARA should behave more like:
- an intelligent operating layer,
- a native daemon,
- an automation engine,

than a traditional chatbot.