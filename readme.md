# 🤖 SARA: Autonomous Cognitive Orchestration Runtime & Windows Agent

SARA (Supervised Agentic Runtime Architecture) is a premium, state-of-the-art autonomous AI agent and WinAPI orchestration runtime designed to grant advanced desktop control, surveillance, and system monitoring directly from your private Telegram chat. 

Unlike traditional reactive chat systems, SARA utilizes a **stateful, dual-engine cognitive architecture** inspired by advanced frameworks like *Devin*, *OpenClaw*, and *Claude Code*. It splits processing into a lightning-fast native C++ system loop and a long-horizon Python task-graph orchestrator to execute multi-step plans, recover from system failures, and handle parallel tasks seamlessly.

---

## 🌟 Key Capabilities & Core Features

*   🧠 **Stateful Dual-Engine Core:** A high-speed, local C++ Windows service (`sara_agent.exe`) handles hardware interfaces, WinAPI hook execution, and event-driven polling. A Python-based AI Worker performs long-horizon DAG planning, self-verification, and failure taxonomy-aware reflection.
*   💻 **Interactive Telegram Docks:** Dynamically rendering in-chat control interfaces using Telegram's inline keyboards and real-time message editing. These allow quick-access system management, media control, system stats, and automated task tracking.
*   ⚙️ **WinAPI & OS Deep Integration:** Complete system command execution, active process management, brightness/volume modification, keystroke injection, camera capturing, and filesystem operations.
*   🖥️ **Desktop Control Panel (ImGui GUI):** A sleek, dark-mode DirectX 11 desktop application (`sara_gui.exe`) providing live logging, dynamic Telegram polling stats, authorization approval, and complete configuration of AI profiles.
*   🔒 **Military-Grade Security Model:** Dual-layer verification containing dynamic host-generated root passwords for new devices, temporary session passwords, and automated Telegram User ID whitelisting with rate-limiting.
*   🌐 **Multi-Provider AI Fallback:** Integrates with OpenAI, Groq, Claude, Gemini, Ollama, and custom endpoints (such as OpenCode AI's DeepSeek) with automated fallback to secondary endpoints if the primary API fails.

---

## 🧠 SARA Cognitive Architecture (The Brain)

SARA’s intelligence is not just a basic system prompt—it is built into its architectural pipeline. When you send a natural language request, SARA routes it through a specialized multi-stage process:

```
[User Message] ──> [Tier Router (0-token Keyword Classifier)]
                         │
                         ├── Tiers 1 & 2 (Volume/Brightness/Media) ──> [Fast Native Execution]
                         │
                         └── Tiers 3 & 4 (Complex Tasks) ───────────> [Python Planner (DAG TaskGraph)]
                                                                               │
                                                        ┌──────────────────────┴──────────────────────┐
                                                        ▼                                             ▼
                                                [WinAPI Execution]                            [Self-Verifier]
                                                        │                                             │
                                                        │ (Failure: Tool/Net/Logic/Auth)              ▼
                                                        └───────────────────────────────────> [Taxonomy-Aware Reflector]
                                                                                                      │ (Max 2 Retries)
                                                                                                      ▼
                                                                                             [Alternative Plan]
```

1.  **Tier Routing (Tiers 1–4):** Categorizes requests to minimize AI token consumption. Tiers 1 and 2 bypass the planner for instant hardware actions (e.g. "mute system"). Tiers 3 and 4 call the planner.
2.  **DAG TaskGraph Planner:** Decomposes complex user goals into dependency-tracked TaskNodes (up to 8 steps). The runtime can execute parallelizable nodes concurrently and handle delayed runs (`delay_seconds`).
3.  **Execution & Tool Result Normalization:** Standardizes all tool results into a strict `{success: bool, data: dict, error: string}` contract to prevent the AI from processing messy raw terminal returns.
4.  **Self-Verification & Failure Taxonomy:** Classifies every failed step into one of six failure categories:
    *   `TOOL_FAILURE` (the tool itself errored)
    *   `PERMISSION_FAILURE` (access denied or needs admin rights)
    *   `NETWORK_FAILURE` (API timeout or packet drop)
    *   `LOGIC_FAILURE` (wrong output generated)
    *   `TIMEOUT` (execution exceeded deadline)
    *   `UNKNOWN` (unclassified error)
5.  **Reflector (Self-Correction):** Spawns dynamic recovery plans. For example, if a `write_file` operation fails with a `PERMISSION_FAILURE`, the Reflector corrects the execution to run an elevated PowerShell script instead of repeatedly executing the failing command.

---

## 💻 The Interactive Docks (Telegram Widgets)

Telegram "Docks" are dynamic dashboard interfaces in your chat. Instead of typing text commands, you interact with active control grids that update in real time.

### 1. 💻 System Dock (`/system` or `/dock system`)
Provides visual commands to control physical hardware and security on the host system:
*   **📸 Screenshot:** Captures the primary desktop screen instantly and posts it directly in chat.
*   **📷 Camera:** Captures a photo using the default connected host webcam and sends it.
*   **🌙 Bright - / ☀️ Bright +:** Decreases or increases system brightness by 20%.
*   **🔒 Lock PC / 🔄 Restart / ⚡ Shutdown:** Triggers safety-guarded OS control. Selecting these prompts a dynamic inline safety confirmation button (`✅ YES` / `❌ CANCEL`) to prevent accidental power-offs.

### 2. 🎵 Media Dock (`/dock` or `/dock media`)
Allows remote control over media playback (works with Spotify, YouTube, VLC, and Windows Media Player):
*   **⏪ 10s / 10s ⏩:** Seeks media forwards or backwards (sends hotkey signals).
*   **⏯ Play/Pause:** Toggles current host playback.
*   **🔉 Vol - / 🔊 Vol +:** Decreases or increases host master volume by 5% increments.
*   **🔇 Mute:** Mutes or unmutes host audio.

### 3. 📊 System Monitor Dock (`/monitor` or `/dock monitor`)
An active monitoring dashboard that generates graphical indicators inside Telegram:
*   **Live Status Bars:** Displays active visual bars `[████░░░░░░]` showing CPU and RAM usage percentage.
*   **Power Source:** Shows battery percentage and charger connection state (`🔌 AC` / `🔋 Battery`).
*   **⚠️ Top Process Monitor:** Detects the heaviest process currently running on the system (by memory consumption) and presents its Name, PID, and exact RAM footprint.
*   **💀 Kill Top Process:** Allows you to instantly terminate the identified top memory hog with a single click.
*   **▶️ Auto-Refresh (15s):** Starts a background thread that dynamically updates the stats widget every 15 seconds for a 1-minute period.

### 4. ⚡ Automation & Tasks Dock (`/rules`, `/tasks`, or `/dock auto`)
Manages background automation:
*   **Active Rules Tracker:** Lists all active event automation rules (e.g. "take screenshot when Discord opens").
*   **Task List:** Lists scheduled or pending timer operations.
*   **Dynamic Actions:** Displays buttons to quickly toggle rules `🟢 Enable / 🔴 Disable` or `❌ Cancel Task` straight from the message.

---

## 📋 Comprehensive Commands Directory

You can talk to SARA using natural, conversational English (e.g., *"SARA, can you take a screenshot and turn down the volume?"*), or you can use target-driven Telegram commands:

### ⚙️ System Commands
*   `/start` — Prints SARA's initialization details, active setup, and lists system examples.
*   `/help` — Displays the primary commands guide.
*   `/status` — Returns a compact, live diagnostic of OS load, active processes, and connection stats.
*   `/ping` — Measures local host loopback latency and returns `🏓 pong`.
*   `/test` — Initiates an exhaustive diagnostic suite testing AI worker API latency, screenshot generation, toast notification capabilities, hardware stats, and network state.

### 🛠 Automation & Tasks
*   `/tasks` — Lists active scheduled timer tasks, displaying their ID, targeting command, and status.
*   `/cancel <task_id>` — Cancels a scheduled task by passing its ID.
*   `/rules` — Lists all registered event-driven rules, showing triggers (e.g., active processes, timers) and their enabled status.

### 📊 Surveillance & Media
*   `/screenshot` — Captures the full screen and uploads it as an uncompressed photo.
*   `/photo` — Triggers the host webcam and uploads the photo.
*   `/monitor` — Displays system load details (CPU, RAM used/total in MB, GPU percent, battery, and process count).

### 🧠 Memory & History Management
*   `/memory` — Lists SARA's long-term user memories stored in the local SQLite database.
*   `/clear` — Clears the conversational memory history from the active session. Spams a series of blank lines to push old conversations out of visual range on mobile devices.
*   `/master_clear` (or `/master clear`) — Performs a complete wipe of the conversational chat history and wipes all long-term database memories.

### 🤖 AI Profile & Configuration Management
*   `/show_config` — Shows the current active AI profile, configured model, provider, custom endpoint, and a masked preview of the active API key.
*   `/set_ai <param> <value>` — Updates settings. `<param>` can be `provider`, `model`, `endpoint`, or `key`. Changes are saved instantly to `settings.json`.
*   `/think` — Enables Deep Reasoning mode. SARA will think deeper before executing tasks.
*   `/nothink` — Disables Reasoning mode. SARA operates with fast, direct response times.

### 🎵 Spotify Control Commands
SARA includes a built-in dedicated Spotify SDK Plugin:
*   `/sp play <song>` — Searches and plays a specific song.
*   `/sp pause` / `/sp resume` — Toggles Spotify playback.
*   `/sp next` / `/sp prev` — Skips tracks.
*   `/sp volume <0-100>` — Modifies Spotify application volume directly.
*   `/sp status` — Shows the currently playing song title, artist name, and album artwork.
*   `/sp heart` — Instantly adds the currently playing song to your "Liked Songs" library.
*   `/sp shuffle <on/off>` — Toggles track shuffling.
*   `/sp repeat <off/all/one>` — Configures repeat settings.
*   `/sp seek <seconds>` — Jumps to a specific timestamp in the song.
*   `/sp dock` — Spawns the dedicated interactive Spotify in-chat controller.

---

## 🛠 Registered AI Tools Catalog (28 Tools)

When SARA decides to use the planner for a Tier 3/4 task, she has direct access to 28 OS-level tools registered within the C++ `ToolRegistry`. Each tool returns a clean JSON response contract to the AI:

| Tool Name | Category | Risk Level | Description |
| :--- | :--- | :--- | :--- |
| `play_youtube` | `media` | `LOW` | Searches and launches a YouTube video in your default browser. |
| `search_google` | `browser` | `LOW` | Performs a Google search query on the host computer. |
| `open_website` | `browser` | `LOW` | Launches a specific web URL in the default browser. |
| `open_app` | `system` | `LOW` | Launches a specific desktop application or executable. |
| `close_process` | `system` | `MED` | Terminates a running process by name or PID. |
| `take_screenshot` | `surveillance` | `LOW` | Captures the active primary monitor and saves it locally. |
| `capture_camera` | `surveillance` | `LOW` | Captures an image from index-0 webcam hardware. |
| `get_system_stats` | `monitoring` | `LOW` | Collects live CPU, RAM load, battery, and GPU metrics. |
| `get_ip_address` | `monitoring` | `LOW` | Returns the local host network IP address. |
| `volume_set` | `system` | `LOW` | Adjusts master system output volume. |
| `volume_mute` | `system` | `LOW` | Toggles hardware system mute state. |
| `change_brightness` | `system` | `LOW` | Adjusts host monitor brightness. |
| `notify` | `communication` | `LOW` | Spawns a native Windows Toast notification on the desktop. |
| `clipboard_write` | `system` | `LOW` | Copies a specified text string into the system clipboard. |
| `clipboard_read` | `system` | `LOW` | Reads the current text string from the system clipboard. |
| `focus_window` | `system` | `LOW` | Searches and focuses a window title on the screen. |
| `send_keys` | `system` | `MED` | Simulates complex key combination clicks on the OS. |
| `run_cmd` | `internal` | `HIGH` | Executes a standard Command Prompt (`cmd.exe`) process. |
| `run_powershell` | `internal` | `HIGH` | Executes a shell script in Windows PowerShell. |
| `write_file` | `internal` | `LOW` | Writes a file (path relative to the user's Desktop). |
| `read_file` | `internal` | `LOW` | Reads text content from a local file (relative to Desktop). |
| `list_dir` | `internal` | `LOW` | Scans and lists files inside a folder (relative to Desktop). |
| `lock_pc` | `system` | `LOW` | Instantly locks the Windows user session workstation. |
| `task_complete` | `internal` | `LOW` | Internal signal showing a multi-step task has finished. |
| `add_event_rule` | `automation` | `LOW` | Registers an event trigger (e.g. process starts -> trigger action). |
| `remove_event_rule`| `automation` | `LOW` | Removes an active event trigger. |
| `list_event_rules` | `automation` | `LOW` | Returns all currently active event triggers. |

---

## 🔒 Security Architecture

To prevent unauthorized access (such as a random Telegram user finding your bot token and accessing your PC), SARA enforces a strict dual-layered authentication shield:

### 1. Root Authorization (Device Whitelisting)
*   When a new, untrusted Telegram User ID sends a message to the bot, SARA immediately **deletes the incoming message** to prevent chat leak.
*   SARA halts the conversation and displays a **Root Authorization Prompt**: *"🔒 Untrusted device. Please provide the Root Password shown on the host computer."*
*   On the host machine, SARA's C++ terminal or GUI displays a dynamically generated **Root Password** (e.g. `test8544uk`).
*   The user must input this exact Root Password on Telegram. Once matched, SARA registers that specific numerical User ID (usernames are ignored to prevent spoofing) into its SQLite Database as a **Trusted User**.

### 2. Session Authentication (Active State Authorization)
*   Every time the bot is restarted, or is left idle, the session is locked.
*   Trusted users are prompted with a lock message: *"🔒 Session locked. Please authenticate using your Session Password."*
*   The user must enter the permanent **Session Password** (configured under `"telegram": {"password": "..."}` inside `settings.json`). SARA deletes the password message instantly upon receipt to keep the chat history secure.

### 3. Rate-Limiting Guard
SARA rate-limits trusted devices. If a script or user attempts to spam messages or flood commands, the bot drops the requests silently to protect system resources.

---

## 🖥️ The Desktop Control Panel (GUI)

SARA features a premium graphical interface built on ImGui and DirectX 11. 

![Control Panel Preview](docs/gui_preview.png) *(Note: Launch `sara_gui.exe` from your build directory)*

*   **Config Tab:** Easily configure your Telegram Bot Token, Session Password, and look up the dynamically generated **Root Password** for new devices.
*   **AI Profiles Manager:** View and create multiple provider setups (Zen, Gemini, Groq, OpenAI). Swap active setups instantly and click **Test AI** to run live system API validation checks.
*   **Users Tab:** View all whitelisted device IDs in a sleek table showing ID, Name, Username, and whitelist date. Admin users can click the **Block** or **Unblock** buttons next to any device to instantly manage host access permissions.
*   **Live Log Tab:** Displays a scrollable terminal showing internal WinAPI events, API payload transactions, and Telegram updates.

---

## ⚙️ Settings & Configuration Guide

All configuration states are stored locally inside `settings.json` in the root project folder. SARA updates this file automatically when you make adjustments in the GUI or via Telegram:

```json
{
  "active_profile": "zen",
  "ai_primary": {
    "api_key": "sk-your-key-here",
    "endpoint": "https://opencode.ai/zen/v1/chat/completions",
    "model": "deepseek-v4-flash-free",
    "provider": "custom"
  },
  "ai_fallback": {
    "api_key": "",
    "endpoint": "",
    "model": "",
    "provider": ""
  },
  "ai_profiles": {
    "zen": {
      "api_key": "sk-your-key-here",
      "endpoint": "https://opencode.ai/zen/v1/chat/completions",
      "model": "deepseek-v4-flash-free",
      "provider": "custom"
    },
    "gemini": {
      "api_key": "gsk_groq_key_here",
      "endpoint": "https://api.groq.com/openai/v1/chat/completions",
      "max_tokens": 16384,
      "model": "openai/gpt-oss-20b",
      "provider": "groq",
      "temperature": 0.7,
      "thinking_enabled": false,
      "timeout_seconds": 120
    }
  },
  "data_dir": "data",
  "log_dir": "logs",
  "log_level": "INFO",
  "scheduler_interval_ms": 500,
  "telegram": {
    "password": "your_secure_session_password",
    "polling_interval_ms": 2000,
    "token": "123456789:ABCDefGhIjKlMnOpQrStUvWxYz"
  }
}
```

---

## 🛠️ Build & Installation Guide

SARA is designed to compile on Windows using MSYS2 (with the UCRT64 toolchain), CMake, and Ninja.

### 📋 Prerequisites
1. Install [MSYS2](https://www.msys2.org/).
2. Run MSYS2 UCRT64 terminal and install dependencies:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sqlite3
   ```
3. Install Python 3.10+ on your system and add it to your system PATH. Install standard HTTP dependencies:
   ```bash
   pip install requests openai nlohmann-json
   ```

### 🔨 Compiling SARA
Navigate to the root SARA directory:
```bash
# Generate Build Files
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile Agent & GUI
cmake --build build --config Release
```

### 🚀 Running SARA
Once built, you can run the applications from the `build/` output folder:
1. Double-click `sara_gui.exe` to open the Control Panel.
2. The GUI will automatically detect and launch `sara_agent.exe` in a silent, background window.
3. Configure your Telegram Token in the Config tab, click **Save & Apply**, and click **Start Polling**.
4. Scan your host machine terminal or the Config tab for the **Pending Root Password** to authorize your mobile device!
