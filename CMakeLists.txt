cmake_minimum_required(VERSION 3.20)
project(sara_agent VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE)

add_executable(sara_agent
    sara_agent/src/main.cpp
    sara_agent/src/Logger.cpp
    sara_agent/src/ConfigManager.cpp
    sara_agent/src/Scheduler.cpp
    sara_agent/src/SQLiteStore.cpp
    sara_agent/src/WinAPIExecutor.cpp
    sara_agent/src/TelegramGateway.cpp
    sara_agent/src/IPCServer.cpp
    sara_agent/src/AIWorkerLauncher.cpp
    sara_agent/src/SessionManager.cpp
    sara_agent/src/ActionValidator.cpp
    sara_agent/src/PermissionManager.cpp
    sara_agent/src/SystemMonitor.cpp
    sara_agent/src/ScreenshotCapture.cpp
    sara_agent/src/CameraCapture.cpp
    # v2.0 — New subsystems
    sara_agent/src/IntentEngine.cpp
    sara_agent/src/ProcessMonitor.cpp
    sara_agent/src/EventAutomation.cpp
    sara_agent/src/HotkeyManager.cpp
    sara_agent/src/NetworkMonitor.cpp
    sara_agent/src/ToolRegistry.cpp
    # Telegram Dock System
    sara_agent/src/DockRouter.cpp
    sara_agent/src/MediaDock.cpp
    sara_agent/src/SystemDock.cpp
    sara_agent/src/MonitorDock.cpp
    sara_agent/src/AutomationDock.cpp
    
    # Native Runtime System
    sara_agent/src/NativeCommandRouter.cpp
    
    # Security System
    sara_agent/src/SecurityManager.cpp

    # Spotify Plugin
    plugins/spotify/spotify_state.cpp
    plugins/spotify/spotify_events.cpp
    plugins/spotify/spotify_ws.cpp
    plugins/spotify/spotify_commands.cpp
    plugins/spotify/spotify_dock.cpp
    plugins/spotify/spotify_plugin.cpp

    # MCP Adapter Layer
    sara_agent/src/plugins/mcp/MCPClient.cpp
    sara_agent/src/plugins/mcp/MCPRegistry.cpp
    sara_agent/src/plugins/mcp/MCPTransport.cpp
)

# Compile sqlite3.c as C, not C++
set_source_files_properties(shared/sqlite3.c PROPERTIES LANGUAGE C)

target_include_directories(sara_agent PRIVATE
    sara_agent/include
    shared
    plugins/spotify
)

target_link_options(sara_agent PRIVATE -static-libstdc++ -static-libgcc -mwindows)

target_link_libraries(sara_agent PRIVATE
    winhttp
    ws2_32
    user32
    gdi32
    shell32
    advapi32
    ole32
    rpcrt4
    winmm
    crypt32
    pdh
    iphlpapi
    mfplat
    mf
    mfreadwrite
    mfuuid
    shlwapi
)

target_sources(sara_agent PRIVATE shared/sqlite3.c)

# GUI executable — Dear ImGui + DirectX 11
add_executable(sara_gui
    sara_gui/src/main.cpp
    sara_gui/src/IPCClient.cpp
    shared/imgui/imgui.cpp
    shared/imgui/imgui_draw.cpp
    shared/imgui/imgui_tables.cpp
    shared/imgui/imgui_widgets.cpp
    shared/imgui/backends/imgui_impl_win32.cpp
    shared/imgui/backends/imgui_impl_dx11.cpp
)

target_include_directories(sara_gui PRIVATE
    sara_gui/include
    shared
    shared/imgui
    shared/imgui/backends
)

target_link_options(sara_gui PRIVATE -static-libstdc++ -static-libgcc -mwindows)

target_link_libraries(sara_gui PRIVATE
    user32
    gdi32
    shell32
    advapi32
    d3d11
    d3dcompiler
    dxgi
    dwmapi
)
