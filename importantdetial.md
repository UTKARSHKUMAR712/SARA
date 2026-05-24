# SARA SYSTEM ENGINEERING DETAILS

# PURPOSE

This document defines:
- internal communication standards,
- action schemas,
- IPC structures,
- event architecture,
- state management,
- failure recovery,
- plugin permissions,
- runtime behavior,
- persistence strategy.

This file acts as:
- the internal language specification of SARA.

Without these standards, the architecture may become inconsistent later.

---

# CORE PRINCIPLE

ALL internal systems must communicate using:
- structured schemas,
- standardized events,
- validated payloads.

Avoid:
- raw strings,
- hardcoded command chains,
- inconsistent formats.

---

# INTERNAL ACTION SCHEMA

# PURPOSE

Every component should use the SAME action format.

This includes:
- AI parser,
- GUI,
- Telegram,
- plugins,
- scheduler,
- external tools,
- MCP integrations.

---

# STANDARD ACTION STRUCTURE

```json
{
  "id": "uuid",
  "source": "telegram",
  "type": "task",
  "action": "close_process",
  "target": "Discord.exe",
  "parameters": {},
  "requires_approval": true,
  "created_at": 0,
  "execute_at": 0,
  "repeat": false,
  "interval_seconds": 0,
  "priority": "normal"
}
```

---

# REQUIRED FIELDS

| Field | Purpose |
|------|----------|
| id | unique identifier |
| source | origin of request |
| type | task/event/plugin/etc |
| action | action name |
| requires_approval | security |
| created_at | timestamps |
| priority | scheduling |

---

# EXAMPLE ACTIONS

## Close app

```json
{
  "action": "close_process",
  "target": "Discord.exe"
}
```

---

## Screenshot

```json
{
  "action": "take_screenshot",
  "repeat": true,
  "interval_seconds": 300
}
```

---

## Notification

```json
{
  "action": "notify",
  "message": "Take rest"
}
```

---

# ACTION VALIDATION

ALL actions must pass:
- schema validation,
- permission validation,
- parameter validation,
- safety checks.

AI output should NEVER directly execute.

---

# IPC PROTOCOL

# PURPOSE

All components communicate using IPC.

Examples:
- GUI ↔ Agent
- Plugins ↔ Agent
- AI Worker ↔ Agent

---

# IPC METHODS

Preferred:
- Named Pipes

Alternative:
- localhost sockets

---

# IPC MESSAGE FORMAT

```json
{
  "source": "gui",
  "destination": "agent",
  "type": "command",
  "request_id": "uuid",
  "payload": {}
}
```

---

# IPC MESSAGE TYPES

| Type | Purpose |
|------|----------|
| command | execute action |
| response | reply |
| event | broadcast |
| status | runtime status |
| error | failures |

---

# EVENT SYSTEM

# PURPOSE

Modules should communicate using events instead of tight coupling.

---

# EXAMPLE EVENTS

```text
MessageReceived
TaskCreated
TaskCompleted
PluginLoaded
CPUHigh
NetworkSpike
PermissionRequested
ScreenshotTaken
```

---

# EVENT STRUCTURE

```json
{
  "event": "TaskCompleted",
  "timestamp": 0,
  "source": "scheduler",
  "payload": {}
}
```

---

# EVENT PRIORITIES

| Priority | Purpose |
|------|----------|
| critical | crashes/security |
| high | automation |
| normal | standard events |
| low | logs/debug |

---

# STATE MANAGEMENT

# PURPOSE

Define how runtime state behaves.

---

# ACTIVE STATE

Stored in memory:
- active tasks
- connected clients
- session context
- plugin states

---

# PERSISTENT STATE

Stored in SQLite:
- scheduled tasks
- repeating tasks
- permissions
- settings
- history

---

# CRASH RECOVERY

On restart:
- reload tasks
- resume repeating tasks
- restore automation state

Tasks should survive:
- crashes
- reboot
- GUI closure

---

# SESSION MEMORY

# PURPOSE

Handle conversational references.

Example:
```text
close it after 5 min
```

Need:
- previous context
- active references

---

# SESSION STRUCTURE

```json
{
  "session_id": "",
  "platform": "telegram",
  "last_targets": [],
  "recent_commands": []
}
```

---

# SESSION CLEANUP

Need:
- automatic expiration
- memory limits
- inactivity cleanup

Avoid:
- infinite context growth

---

# FAILURE RECOVERY SYSTEM

# PURPOSE

Handle failures gracefully.

---

# FAILURE TYPES

| Failure | Strategy |
|------|----------|
| AI timeout | retry/fallback |
| Telegram disconnect | reconnect |
| plugin crash | isolate/restart |
| WinAPI failure | retry/log |
| scheduler failure | restore state |

---

# STANDARD FAILURE FLOW

```text
Failure
   ↓
Retry
   ↓
Fallback
   ↓
Log
   ↓
Notify User
```

---

# RETRY POLICY

Need:
- retry limits
- exponential backoff
- cooldowns

Avoid:
- infinite loops

---

# PLUGIN SYSTEM DETAILS

# PURPOSE

Allow modular expansion safely.

---

# PLUGIN TYPES

Supported:
- executable plugins
- API plugins
- Python connectors
- MCP-style connectors

Avoid initially:
- direct DLL injection

---

# PLUGIN MANIFEST

```json
{
  "name": "netsense",
  "version": "1.0",
  "permissions": [
    "network_read"
  ],
  "commands": [
    "network_stats"
  ]
}
```

---

# PLUGIN PERMISSIONS

Examples:
- network_read
- system_read
- shell_execute
- screenshot_access
- filesystem_access

Plugins should only access approved capabilities.

---

# PLUGIN ISOLATION

Plugins should:
- run separately
- communicate via IPC/API
- avoid direct memory access

Reason:
- stability
- security
- crash isolation

---

# AI PROVIDER NORMALIZATION

# PURPOSE

Different AI APIs behave differently.

Need one unified internal format.

---

# INTERNAL MESSAGE FORMAT

```json
{
  "role": "user",
  "content": "Close Discord after 50 min"
}
```

ALL providers convert to/from this format internally.

---

# AI RESPONSE FORMAT

```json
{
  "success": true,
  "structured_action": {}
}
```

---

# PROVIDER DIFFERENCES TO HANDLE

| Provider | Difference |
|------|----------|
| OpenAI | standard chat |
| Claude | different formatting |
| Gemini | different tools |
| Ollama | local APIs |
| Groq | OpenAI-like |
| NVIDIA NIM | OpenAI-like mostly |

---

# RATE LIMITING

# PURPOSE

Prevent spam and overload.

---

# NEED LIMITS FOR

- Telegram
- Discord
- plugins
- AI requests
- screenshots
- notifications

---

# EXAMPLE LIMITS

| Action | Limit |
|------|----------|
| screenshots | max per minute |
| AI requests | cooldown |
| shell commands | approval required |

---

# SECURITY MODEL

# MOST IMPORTANT RULE

AI NEVER directly executes.

Always:
```text
AI
 ↓
Validator
 ↓
Permission Layer
 ↓
Executor
```

---

# APPROVAL SYSTEM

High-risk actions require:
- popup confirmation
- Telegram confirmation
- GUI approval

Examples:
- PowerShell
- CMD
- registry edits
- filesystem deletion

---

# PRIVILEGE MANAGEMENT

Need:
- detect admin mode
- elevation prompts
- permission fallback

Some WinAPI features require admin rights.

---

# STORAGE STRUCTURE

# SQLITE TABLES

## tasks

```text
id
action
target
execute_at
repeat
interval
status
```

---

## permissions

```text
plugin
permission
allowed
```

---

## sessions

```text
session_id
platform
context
```

---

# LOGGING SYSTEM

Need:
- rotating logs
- log levels
- debug mode
- crash logging

Avoid:
- excessive disk writes

---

# THREADING MODEL

# IMPORTANT

Avoid:
- thread-per-task

Preferred:
- thread pools
- event loops
- async queues

Reason:
- lower RAM
- better stability

---

# WINDOWS EXECUTION MODEL

SARA should initially run as:
- normal user-mode process

NOT:
- Windows service

Reason:
- WinAPI desktop interaction limitations

---

# UPDATE STRATEGY

Future support:
- self-updates
- plugin updates
- config migration

Need:
- version compatibility checks

---

# PACKAGING STRATEGY

Potential:
- portable version
- installer version
- startup registration

Need:
- dependency minimization
- lightweight deployment

---

# PERFORMANCE TARGETS

| Metric | Goal |
|------|----------|
| idle RAM | extremely low |
| idle CPU | near zero |
| thread count | minimal |
| startup time | fast |
| screenshot overhead | controlled |

---

# MOST IMPORTANT ENGINEERING RULES

## DO NOT:
- tightly couple modules
- let GUI own runtime
- let AI directly execute
- create many permanent processes
- overuse threads
- overuse polling

---

# ALWAYS:
- validate actions
- clean resources
- isolate plugins
- normalize APIs
- log failures
- prioritize lightweight architecture

---

# FINAL ENGINEERING MINDSET

Think like:
- system software engineer
- daemon developer
- runtime architect
- OS tooling developer

NOT like:
- web app developer
- Electron app builder
- chatbot wrapper creator

SARA succeeds ONLY if:
- runtime stays lightweight
- architecture stays modular
- execution remains safe
- systems remain isolated
- automation remains stable