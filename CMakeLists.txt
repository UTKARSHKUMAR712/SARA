cmake_minimum_required(VERSION 3.20)
project(sara_agent VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_C_FLAGS_RELEASE "-O3")

add_definitions(-DUNICODE -D_UNICODE)

add_executable(sara_agent
    sara_agent/src/main.cpp
    sara_agent/src/AgentInitializer.cpp
    sara_agent/src/Logger.cpp
    sara_agent/src/ConfigManager.cpp
    sara_agent/src/ActionDispatcher.cpp
    sara_agent/src/MessageHandlers.cpp
    sara_agent/src/Scheduler.cpp
    sara_agent/src/SQLiteStore.cpp
    sara_agent/src/WinAPIExecutor.cpp
    sara_agent/src/TelegramGateway.cpp
    sara_agent/src/IPCServer.cpp
    sara_agent/src/ActionValidator.cpp
    sara_agent/src/PermissionManager.cpp
    sara_agent/src/SystemMonitor.cpp
    sara_agent/src/ScreenshotCapture.cpp
    sara_agent/src/CameraCapture.cpp
    # v2.0 — New subsystems
    sara_agent/src/ProcessMonitor.cpp
    sara_agent/src/EventAutomation.cpp
    sara_agent/src/HotkeyManager.cpp
    sara_agent/src/NetworkMonitor.cpp
    sara_agent/src/ToolRegistry.cpp
    sara_agent/src/CommandMap.cpp
    
    # v3.0 — Core Runtime
    sara_agent/src/WebSocketServer.cpp
    sara_agent/src/RuntimeState.cpp
    
    # Telegram Dock System
    sara_agent/src/DockRouter.cpp
    sara_agent/src/MediaDock.cpp
    plugins/media/media_service.cpp
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

    # Plugin System
    sara_agent/src/PluginManager.cpp
    
    # MCP Adapter Layer
    sara_agent/src/plugins/mcp/MCPClient.cpp
    sara_agent/src/plugins/mcp/MCPRegistry.cpp
    sara_agent/src/plugins/mcp/MCPTransport.cpp

    # ── Remote Terminal Runtime (Phase 1–4) ───────────────────────────────────
    remote_runtime/src/TerminalToken.cpp
    remote_runtime/src/ConPTYSession.cpp
    remote_runtime/src/TerminalSessionManager.cpp
    remote_runtime/src/TerminalHttpServer.cpp
    remote_runtime/src/CloudflaredManager.cpp
    remote_runtime/src/FileBrowserManager.cpp
    remote_runtime/src/PortDetector.cpp
    remote_runtime/src/LiveServer.cpp
)

# Compile sqlite3.c as C, not C++
set_source_files_properties(shared/sqlite3.c PROPERTIES LANGUAGE C)

target_compile_options(sara_agent PRIVATE -flto -s)
target_link_options(sara_agent PRIVATE -flto -s)

target_include_directories(sara_agent PRIVATE
    sara_agent/include
    shared
    plugins/spotify
    plugins/media
    remote_runtime/include
    C:/msys64/ucrt64/include/winrt
    C:/msys64/ucrt64/include
)

target_compile_options(sara_agent PRIVATE
    -std=gnu++20
    -fcoroutines
    -DWINRT_LEAN_AND_MEAN
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
    bcrypt
    windowsapp
)

# Embed UAC admin manifest so sara_agent.exe always requests elevation
if(WIN32)
    target_sources(sara_agent PRIVATE sara_agent/sara_agent.manifest)
    set_source_files_properties(sara_agent/sara_agent.manifest PROPERTIES
        VS_TOOL_OVERRIDE "MT"
    )
    # For MinGW/GCC: embed manifest via windres
    if(MINGW)
        set(MANIFEST_RC "${CMAKE_BINARY_DIR}/sara_agent_manifest.rc")
        file(WRITE "${MANIFEST_RC}"
            "#include <windows.h>\n"
            "CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST \"${CMAKE_SOURCE_DIR}/sara_agent/sara_agent.manifest\"\n"
        )
        target_sources(sara_agent PRIVATE "${MANIFEST_RC}")
    endif()
endif()

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

target_compile_options(sara_gui PRIVATE -static-libstdc++ -static-libgcc -mwindows)
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
