# SARA Remote Desktop Capabilities

## Overview

SARA provides comprehensive, full-featured remote KVM (Keyboard, Video, Mouse) access, allowing the user to view their desktop, control the mouse and keyboard, access files, and manage background services from any device, anywhere in the world. Instead of reinventing a proprietary remote desktop protocol, SARA seamlessly integrates the **MeshCentral** ecosystem, managing its backend services and tunneling network traffic through **Cloudflare** for secure public access.

## Architecture & Ecosystem

SARA relies on the following components to power its remote desktop feature:
1. **SARA Agent (`sara_agent.exe`)**: The C++ core application that acts as the orchestrator.
2. **MeshCentral Server (Node.js)**: A local instance of the open-source web-based remote management server.
3. **MeshAgent (`MeshAgent.exe`)**: The native background service running on the host machine that captures KVM events, telemetry, and terminal I/O.
4. **Cloudflare Tunnel (`cloudflared.exe`)**: Creates a secure outbound connection from the host PC to the Cloudflare Edge, giving the local MeshCentral server a public HTTPS URL without needing port forwarding or static IPs.

### System Data Flow

```mermaid
flowchart TD
    User([User (Browser)]) <-->|WebRTC / WebSocket KVM| CloudflareEdge[Cloudflare Edge]
    TGUser([User (Telegram)]) -->|Sends '/desktop'| SARA_Bot[Telegram Gateway]
    
    CloudflareEdge <-->|Encrypted Tunnel| Cloudflared[Cloudflared Daemon]
    
    subgraph Host PC
        SARA_Bot --> Router[NativeCommandRouter]
        Router --> MCM[MeshCentralManager]
        
        MCM -->|1. Start| MeshCentral[MeshCentral Server (Node.js)]
        MCM -->|2. Request Tunnel| Cloudflared
        Router -->|3. System Exec| MeshAgent[MeshAgent Service]
        
        MeshAgent <-->|Local WebSockets| MeshCentral
        Cloudflared <-->|Local Port Forward| MeshCentral
    end
    
    MeshCentral -.->|Replies with Public URL| Router
    Router -.->|Sends Link| TGUser
```

## How It Works: The Execution Pipeline

When the user triggers remote access via the Telegram bot, the following sequence occurs within SARA:

### 1. Command Routing
The user types `/desktop` in their Telegram chat. The `TelegramGateway` passes this message to the `NativeCommandRouter`, which intercepts it and spins up an asynchronous background thread. This ensures the main polling loop for Telegram remains unblocked.

### 2. Node.js & Environment Verification
The orchestrator object, `MeshCentralManager`, is invoked. 
- It first verifies if the host system has a compatible, globally installed version of **Node.js** (v20 or lower). 
- If an incompatible version is found or Node.js is missing entirely, SARA automatically downloads and extracts a portable `v20.11.1` Node.js binary into its internal `data/nodejs/` directory.
- It then checks if the `meshcentral` NPM package (specifically `v1.1.26`) is installed in `sara\meshcentral\node_modules\`. If missing, it installs it silently via `npm install`.

### 3. Starting the MeshCentral Backend
Once verified, `MeshCentralManager` spawns the MeshCentral Node.js process using the native WinAPI `CreateProcessA`. The server binds to a specified local port (defaulting to 443 or defined in SARA's config) and loads its configuration from `meshcentral-data/config.json`.
A dedicated background thread is created to monitor the `proc_handle_`. If the MeshCentral server crashes, the manager updates its internal state.

### 4. Tunneling via Cloudflare
SARA leverages its `CloudflaredManager` to request a **Quick Tunnel** targeting the local MeshCentral port. 
By executing `cloudflared.exe tunnel --url https://localhost:<PORT>`, SARA generates a temporary public `.trycloudflare.com` domain. The manager intercepts the `stdout`/`stderr` of the Cloudflared process to extract this randomly generated URL.

### 5. Activating the MeshAgent
SARA executes a system call to start the local endpoint client:
`system("C:\\Users\\utkarsh_kumar\\Desktop\\MeshAgent.exe start");`
The `MeshAgent.exe` reads its `.msh` configuration file (which binds it to the local MeshCentral server) and connects over local WebSockets. The agent is responsible for screen scraping, input injection, and file system bridging.

### 6. User Handoff
SARA waits in a polling loop (up to 30 seconds) until the Cloudflare URL is successfully acquired. Once acquired, it delays an additional 2 seconds to allow Cloudflare Edge routes to stabilize, then replies to the user on Telegram with both the **LAN URL** and **Public KVM URL**.

## Teardown and Graceful Shutdown

Because remote desktop environments and tunnels consume considerable resources and bandwidth, SARA provides a teardown mechanism via the `/desktopshutdown` command.

When invoked, the `NativeCommandRouter`:
1. Calls `MeshCentralManager::stop()`, which utilizes `TerminateProcess` to kill the Node.js KVM server and closes process handles.
2. Stops the `CloudflaredManager` instance running the MeshCentral tunnel.
3. Issues aggressive `taskkill` commands as a fallback to ensure no stray `node.exe` or `cloudflared.exe` zombie processes remain.
4. Executes `system("C:\\Users\\utkarsh_kumar\\Desktop\\MeshAgent.exe stop");` to halt the background telemetry and screen-capture service.

## MeshCentral Data & Configuration

The user's local `sara/meshcentral` directory structure isolates the server logic:
- `meshcentral-data/`: Contains the critical SQLite databases (`meshcentral.db`, `meshcentral-events.db`), TLS/SSL certificates (`webserver-cert-public.crt`, `root-cert-private.key`), and `config.json`.
- `meshagent64-MySARAPCs.exe`: Often bundled or auto-generated by the server based on the local system architecture, representing the deployment client. (In SARA's current configuration, the active agent binary is located on the user's Desktop).

This decoupled architecture allows SARA to retain the immense power of an enterprise-grade KVM tool while completely abstracting the complex deployment, networking, and lifecycle management away from the end-user.
