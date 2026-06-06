# SARA Workspace Plugin — GitHub Codespaces Style IDE

Create a brand-new module named **SARA Workspace**.

This is NOT a modification of the existing Terminal UI and NOT a modification of the existing File Browser UI.

The Workspace must be implemented as a completely independent plugin/module that integrates into SARA through clearly defined interfaces. Existing `/terminal` and `/files` functionality must continue working exactly as they do today.

---

## Vision

The goal is to create a self-hosted development environment similar to GitHub Codespaces, except everything runs locally on the user's laptop.

Target experience:

* VS Code style layout
* Mobile friendly
* Runs through the existing SARA authentication system
* Runs through the existing Cloudflare tunnel
* Uses local machine resources
* No AI integration yet
* No Git integration yet
* Architecture must reserve space for future AI and Git integration

The Workspace should feel like a lightweight Codespaces clone.

---

# Architectural Requirements

Create a new independent module:

```text
/workspace
```

This must be separate from:

```text
/files
/terminal
/chat
```

These existing modules must remain fully operational and unchanged.

Workspace must function as a plugin-like component attached to SARA, not as a replacement for existing modules.

---

# Workspace Layout

Implement a VS Code style layout:

```text
┌─┬──────────────┬─────────────────────┐
│ │ Explorer     │ Monaco Editor       │
│ │              │                     │
│ │ File Tree    │ Open Files/Tabs     │
│ │              │                     │
├─┴──────────────┴─────────────────────┤
│           Terminal Panel             │
└──────────────────────────────────────┘
```

---

# Activity Bar

Create a left activity bar similar to VS Code.

Initial icons:

```text
Explorer
Terminal
Ports
Settings
```

Future placeholders:

```text
Git
AI
Extensions
```

These future sections should not be implemented yet, but architecture must support adding them later without major redesign.

---

# Explorer

Do NOT embed the existing File Browser UI.

Instead:

* Use File Browser as the filesystem backend
* Use File Browser APIs internally
* Build a custom lightweight VS Code style explorer

Requirements:

* Open Folder
* Create File
* Create Folder
* Rename
* Delete
* Move
* Refresh
* Expand/Collapse folders
* Context menus
* File tree navigation

Example:

```text
📁 src
  📄 main.cpp
  📄 app.cpp

📁 include
  📄 app.h

📄 README.md
```

---

# Monaco Editor

Use Monaco Editor.

Requirements:

* Multiple tabs
* Open file
* Save file
* Dirty file indicators
* Syntax highlighting
* Search
* Themes
* Large file support

Supported initially:

```text
cpp
h
hpp
py
js
ts
json
html
css
md
txt
```

Editor should consume files through the explorer/backend abstraction.

---

# Terminal Panel

Reuse existing SARA terminal infrastructure:

* TerminalSessionManager
* ConPTY
* Existing terminal backend
* Existing websocket infrastructure

Do NOT embed the existing terminal page.

Create a dedicated workspace terminal panel.

Requirements:

* Multiple terminal tabs
* Resize support
* Reconnect support
* Mobile support

---

# Ports Panel

Implement a Codespaces-style Ports view.

Purpose:

Detect and manage locally running web services.

Examples:

```text
3000
5173
8000
8080
```

Workspace should provide:

* Port list
* Running status
* Open Preview action
* Copy Preview URL action

Preview URLs must use the existing Cloudflare tunnel infrastructure.

Architecture should support:

```text
/preview/{port}
```

style routing through SARA.

---

# Mobile First Design

This workspace will primarily be used from a phone.

Requirements:

* Responsive layout
* Collapsible explorer
* Collapsible terminal
* Touch-friendly controls
* Fast rendering
* Preserve editor space

Default mobile state:

* Explorer collapsed
* Terminal collapsed
* Editor receives maximum space

---

# Separation of Concerns

Workspace must be isolated from existing modules.

Rules:

* No modifications to existing File Browser UI
* No modifications to existing Terminal UI
* No breaking changes to current functionality
* Workspace consumes services through interfaces/APIs

Think of Workspace as:

```text
Plugin
  ↓
Uses File Service
Uses Terminal Service
Uses Port Service
```

not:

```text
Workspace directly owns everything
```

---

# Future Expansion

Reserve architecture for:

```text
Git Integration
AI Integration
Extension System
Search System
```

but do not implement them now.

Only create extension points/interfaces where appropriate.

---

# Development Order

Phase 1

* Workspace shell
* Activity bar
* Monaco integration
* Explorer
* Open file
* Save file
* Multiple tabs

Phase 2

* Workspace terminal panel
* Terminal tabs
* Mobile optimization

Phase 3

* Ports panel
* Preview URL system
* Port detection

Phase 4

* Settings panel
* Workspace persistence
* Layout persistence

---

# Final Goal

Deliver a local-first, self-hosted development environment that feels very similar to GitHub Codespaces while remaining:

* Independent from existing SARA modules
* Powered by local machine resources
* Accessible through the existing SARA authentication system
* Accessible through the existing Cloudflare tunnel
* Modular enough for future Git and AI integration
* Optimized for mobile development workflows
