#include "../include/WinAPIExecutor.h"
#include "../include/ScreenshotCapture.h"
#include "../include/CameraCapture.h"
#include "../include/SystemMonitor.h"
#include "../include/Logger.h"
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <mmsystem.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <sstream>
#include <thread>
#include <vector>
#include <filesystem>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <cstdlib>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace sara {

ActionResult WinAPIExecutor::execute(const std::string& action, const json& params) {
    if (action == "open_app") {
        return open_app(params.value("target", params.value("name", "")), params);
    }
    if (action == "close_process") {
        return close_process(params.value("target", params.value("name", "")), params);
    }
    if (action == "enum_processes") {
        return enum_processes();
    }
    if (action == "focus_window") {
        return focus_window(params.value("target", params.value("title", "")));
    }
    if (action == "enum_windows") {
        return enum_windows(params.value("filter", ""));
    }
    if (action == "send_keys" || action == "type_text") {
        return send_keys(params.value("keys", params.value("text", params.value("target", ""))));
    }
    if (action == "run_cmd") {
        return run_cmd(params.value("command",
            params.value("cmd", params.value("target", ""))),
            params.value("timeout", 30));
    }
    if (action == "run_powershell") {
        return run_powershell(params.value("command",
            params.value("cmd", params.value("target", ""))),
            params.value("timeout", 30));
    }
    if (action == "notify") {
        return notify(params.value("target", params.value("title", "SARA")),
            params.value("message", params.value("text", "")));
    }
    if (action == "media_play_pause") {
        keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0); keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
        return {true, "Play/Pause", {}};
    }
    if (action == "media_next") {
        keybd_event(VK_MEDIA_NEXT_TRACK, 0, 0, 0); keybd_event(VK_MEDIA_NEXT_TRACK, 0, KEYEVENTF_KEYUP, 0);
        return {true, "Next Track", {}};
    }
    if (action == "media_prev") {
        keybd_event(VK_MEDIA_PREV_TRACK, 0, 0, 0); keybd_event(VK_MEDIA_PREV_TRACK, 0, KEYEVENTF_KEYUP, 0);
        return {true, "Prev Track", {}};
    }
    if (action == "media_stop") {
        keybd_event(VK_MEDIA_STOP, 0, 0, 0); keybd_event(VK_MEDIA_STOP, 0, KEYEVENTF_KEYUP, 0);
        return {true, "Stop", {}};
    }
    if (action == "lock_pc") {
        LockWorkStation();
        return {true, "PC Locked", {}};
    }
    if (action == "sleep_pc") {
        HMODULE hPowrProf = LoadLibraryA("powrprof.dll");
        if (hPowrProf) {
            typedef BOOLEAN(WINAPI * PSetSuspendState)(BOOLEAN, BOOLEAN, BOOLEAN);
            PSetSuspendState SetSuspendStateFn = (PSetSuspendState)GetProcAddress(hPowrProf, "SetSuspendState");
            if (SetSuspendStateFn) {
                SetSuspendStateFn(FALSE, FALSE, FALSE);
            }
            FreeLibrary(hPowrProf);
        }
        return {true, "PC Sleeping", {}};
    }
    if (action == "restart_pc") {
        system("shutdown /r /t 0");
        return {true, "Restarting PC", {}};
    }
    if (action == "shutdown_pc") {
        system("shutdown /s /t 0");
        return {true, "Shutting down PC", {}};
    }
    if (action == "logoff_pc") {
        ExitWindowsEx(EWX_LOGOFF, 0);
        return {true, "Logging off", {}};
    }
    if (action == "show_desktop") {
        keybd_event(VK_LWIN, 0, 0, 0); keybd_event('D', 0, 0, 0);
        keybd_event('D', 0, KEYEVENTF_KEYUP, 0); keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
        return {true, "Desktop toggled", {}};
    }
    if (action == "close_active") {
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return {true, "Active window closed", {}};
        }
        return {false, "No active window", {}};
    }
    if (action == "monitor_off") {
        SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
        return {true, "Monitor off", {}};
    }
    if (action == "start_screensaver") {
        SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_SCREENSAVE, 0);
        return {true, "Screensaver started", {}};
    }
    if (action == "volume_set") {
        return volume_set(params.value("level", 50));
    }
    if (action == "volume_mute") {
        return volume_mute(params.value("mute", true));
    }
    if (action == "clipboard_write") {
        return clipboard_write(params.value("text", params.value("target", "")));
    }
    if (action == "clipboard_read") {
        return clipboard_read();
    }
    if (action == "open_url" || action == "open_file") {
        std::string url = params.value("url", params.value("target", params.value("path", "")));
        if (url.empty()) return {false, "No URL specified", {}};
        HINSTANCE r = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)r > 32) return {true, "Opened: " + url, {}};
        // Fallback: use cmd start for stubborn file/url opens
        std::string cmd = "cmd.exe /c start \"\" \"" + url + "\"";
        int sys_res = std::system(cmd.c_str());
        if (sys_res == 0) return {true, "Opened (fallback): " + url, {}};
        return {false, "Failed to open: " + url, {}};
    }
    // Handle media_command by dispatching to sub-action
    if (action == "media_command") {
        std::string sub_act = params.value("action", "");
        if (sub_act.empty()) return {false, "media_command: missing 'action' field", {}};
        json sub_params = params;
        sub_params.erase("action");
        return execute(sub_act, sub_params);
    }
    // Spotify sub-actions — handled by C++ after being dispatched here
    // (spotify_play etc. are handled by ToolRegistry via SpotifyCommands)
    // These pass-through to the Unknown action fallback so ToolRegistry picks them up
    if (action == "take_screenshot") {
        ScreenshotCapture sc;
        auto r = sc.capture_fullscreen(params.value("output_dir", "data\\screenshots"));
        return {r.success, r.success ? "Screenshot saved: " + r.file_path : r.error,
                {{"path", r.file_path}, {"width", r.width}, {"height", r.height}}};
    }
    if (action == "capture_camera" || action == "take_photo") {
        CameraCapture cc;
        int idx = params.value("device_index", 0);
        auto r = cc.capture(params.value("output_dir", "data\\screenshots"), idx);
        return {r.success, r.success ? "Photo saved: " + r.file_path : r.error,
                {{"path", r.file_path}}};
    }
    if (action == "get_system_stats") {
        SystemMonitor mon;
        return {true, "System stats retrieved", mon.get_system_summary()};
    }
    if (action == "get_ip_address") {
        return get_ip_address();
    }
    // ── New actions ─────────────────────────────────────────────────
    if (action == "play_youtube") {
        return play_youtube(params.value("query", params.value("target", "")));
    }
    if (action == "search_google") {
        return search_google(params.value("query", params.value("target", "")));
    }
    if (action == "open_website") {
        return open_website(params.value("url", params.value("target", "")));
    }
    if (action == "change_brightness") {
        return change_brightness(params.value("level", 50));
    }
    if (action == "write_file") {
        return write_file(params.value("path", ""), params.value("content", ""));
    }
    if (action == "read_file") {
        return read_file(params.value("path", ""));
    }
    if (action == "list_dir") {
        return list_dir(params.value("path", ""));
    }
    if (action == "clean_memory") {
        auto res = run_cmd("cd /d \"C:\\Users\\utkarsh_kumar\\Desktop\\sara\\Mem Reduct\" && memreduct.exe -clean:3 -exit", 30);
        return {res.success, res.success ? "🧹 Memory cleaned successfully (Working Set + Standby Lists)!" : "❌ Failed to clean memory.", {}};
    }
    if (action == "lock_pc") {
        return lock_pc();
    }
    if (action == "add_event_rule") {
        // Handled in main.cpp — executor returns placeholder
        return {true, "Event rule request received", {}};
    }
    if (action == "remove_event_rule") {
        return {true, "Event rule removal request received", {}};
    }
    if (action == "list_event_rules") {
        return {true, "Event rule list request received", {}};
    }

    return {false, "Unknown action: " + action, {}};
}

ActionResult WinAPIExecutor::open_app(const std::string& target, const json& params) {
    if (target.empty()) return {false, "No target specified", {}};

    std::string actual_target = target;
    // Map common aliases
    std::string lower_target = target;
    std::transform(lower_target.begin(), lower_target.end(), lower_target.begin(), ::tolower);
    if (lower_target == "edge") actual_target = "msedge.exe";
    else if (lower_target == "chrome") actual_target = "chrome.exe";
    else if (lower_target == "firefox") actual_target = "firefox.exe";
    else if (lower_target == "spotify") {
        std::system("start spotify:");
        Logger::instance().info("Opened app: spotify using system command");
        return {true, "Opened: spotify", {}};
    }
    else if (lower_target == "notepad") actual_target = "notepad.exe";
    else if (lower_target == "calculator" || lower_target == "calc") actual_target = "calc.exe";
    else if (lower_target == "cmd") actual_target = "cmd.exe";
    else if (lower_target == "powershell") actual_target = "powershell.exe";

    std::string args = params.value("args", "");
    HINSTANCE result = ShellExecuteA(nullptr, "open", actual_target.c_str(),
        args.empty() ? nullptr : args.c_str(), nullptr, SW_SHOWNORMAL);

    if ((INT_PTR)result <= 32) {
        // Fallback: use cmd /c start which uses Windows registry App Paths
        std::string cmd_target = actual_target;
        // Strip .exe for 'start' command (it works better with app names)
        if (cmd_target.size() >= 4 &&
            cmd_target.substr(cmd_target.size() - 4) == ".exe") {
            cmd_target = cmd_target.substr(0, cmd_target.size() - 4);
        }
        std::string cmd = "cmd.exe /c start \"\" " + cmd_target;
        if (!args.empty()) cmd += " " + args;
        int sys_res = std::system(cmd.c_str());
        if (sys_res == 0) {
            Logger::instance().info("Opened app via cmd fallback: " + cmd_target);
            return {true, "Opened (via shell): " + cmd_target, {}};
        }
        Logger::instance().warning("Failed to open app: " + target + " (ShellExecute + cmd both failed)");
        return {false, "Failed to open: " + target, {}};
    }
    Logger::instance().info("Opened app: " + actual_target);
    return {true, "Opened: " + actual_target, {}};
}

ActionResult WinAPIExecutor::close_process(const std::string& target, const json& params) {
    if (target.empty()) return {false, "No target specified", {}};

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {false, "Failed to create process snapshot", {}};
    }

    PROCESSENTRY32W pe = { sizeof(pe) };
    std::wstring wtarget(target.begin(), target.end());
    std::transform(wtarget.begin(), wtarget.end(), wtarget.begin(), ::towlower);
    bool found = false;
    int killed = 0;

    if (Process32FirstW(snapshot, &pe)) {
        do {
            std::wstring wexe = pe.szExeFile;
            std::transform(wexe.begin(), wexe.end(), wexe.begin(), ::towlower);
            
            bool match = false;
            if (wexe == wtarget || wexe == wtarget + L".exe") match = true;
            else if (wtarget.length() >= 4 && wexe.find(wtarget) != std::wstring::npos) match = true;

            if (match) {
                HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (proc) {
                    if (TerminateProcess(proc, 0)) {
                        killed++;
                    }
                    CloseHandle(proc);
                }
                found = true;
            }
        } while (Process32NextW(snapshot, &pe));
    }
    CloseHandle(snapshot);

    if (!found) return {false, "Process not found: " + target, {}};
    Logger::instance().info("Closed " + std::to_string(killed) + " instance(s) of: " + target);
    return {true, "Closed " + std::to_string(killed) + " instance(s)", {}};
}

ActionResult WinAPIExecutor::enum_processes() {
    DWORD processes[1024], needed;
    if (!EnumProcesses(processes, sizeof(processes), &needed)) {
        return {false, "Failed to enumerate processes", {}};
    }

    json list = json::array();
    DWORD count = needed / sizeof(DWORD);
    for (DWORD i = 0; i < count; i++) {
        if (processes[i] == 0) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
        if (h) {
            WCHAR name[MAX_PATH];
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(h, 0, name, &size)) {
                std::wstring wname(name);
                std::string sname(wname.begin(), wname.end());
                auto pos = sname.find_last_of("\\/");
                if (pos != std::string::npos) sname = sname.substr(pos + 1);
                list.push_back({{"pid", (int)processes[i]}, {"name", sname}});
            }
            CloseHandle(h);
        }
    }
    return {true, "Processes enumerated", list};
}

struct EnumWindowData {
    std::string filter;
    json results = json::array();
};

static BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM lparam) {
    auto* data = reinterpret_cast<EnumWindowData*>(lparam);
    if (!IsWindowVisible(hwnd)) return TRUE;

    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));
    if (strlen(title) == 0) return TRUE;

    if (!data->filter.empty()) {
        std::string stitle(title);
        if (stitle.find(data->filter) == std::string::npos) return TRUE;
    }

    RECT rect;
    GetWindowRect(hwnd, &rect);
    data->results.push_back({
        {"hwnd", (int)(INT_PTR)hwnd},
        {"title", title},
        {"left", rect.left}, {"top", rect.top},
        {"right", rect.right}, {"bottom", rect.bottom}
    });
    return TRUE;
}

struct FocusData {
    std::string target;
    HWND found;
};

static BOOL CALLBACK focus_enum_proc(HWND hwnd, LPARAM lparam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    char title[512];
    GetWindowTextA(hwnd, title, sizeof(title));
    if (strlen(title) == 0) return TRUE;

    std::string lowerTitle = title;
    for (auto& c : lowerTitle) c = ::tolower((unsigned char)c);

    FocusData* data = reinterpret_cast<FocusData*>(lparam);
    std::string lowerTarget = data->target;
    for (auto& c : lowerTarget) c = ::tolower((unsigned char)c);
    
    if (lowerTitle.find(lowerTarget) != std::string::npos) {
        data->found = hwnd;
        return FALSE;
    }
    return TRUE;
}

ActionResult WinAPIExecutor::focus_window(const std::string& title) {
    FocusData data{title, nullptr};
    EnumWindows(focus_enum_proc, reinterpret_cast<LPARAM>(&data));
    if (!data.found) return {false, "Window not found: " + title, {}};
    
    HWND hwnd = data.found;
    
    keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | 0, 0);
    SetForegroundWindow(hwnd);
    keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    
    BringWindowToTop(hwnd);
    
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }
    
    return {true, "Focused window matching: " + title, {}};
}

ActionResult WinAPIExecutor::enum_windows(const std::string& filter) {
    EnumWindowData data;
    data.filter = filter;
    EnumWindows(enum_windows_proc, reinterpret_cast<LPARAM>(&data));
    return {true, "Windows enumerated", data.results};
}

ActionResult WinAPIExecutor::send_keys(const std::string& keys) {
    INPUT inputs[512];
    ZeroMemory(inputs, sizeof(inputs));
    int count = 0;

    for (size_t i = 0; i < keys.size() && count < 510; i++) {
        WORD vk = 0;
        if (keys[i] == '{') {
            auto close = keys.find('}', i);
            if (close == std::string::npos) continue;
            std::string special = keys.substr(i + 1, close - i - 1);
            i = close;

            if (special == "ENTER") vk = VK_RETURN;
            else if (special == "TAB") vk = VK_TAB;
            else if (special == "ESC") vk = VK_ESCAPE;
            else if (special == "BACKSPACE") vk = VK_BACK;
            else if (special == "SPACE") vk = VK_SPACE;
            else if (special == "UP") vk = VK_UP;
            else if (special == "DOWN") vk = VK_DOWN;
            else if (special == "LEFT") vk = VK_LEFT;
            else if (special == "RIGHT") vk = VK_RIGHT;
            else if (special == "CTRL") vk = VK_CONTROL;
            else if (special == "SHIFT") vk = VK_SHIFT;
            else if (special == "ALT") vk = VK_MENU;
            else if (special == "VK_MEDIA_PLAY_PAUSE") vk = VK_MEDIA_PLAY_PAUSE;
            else if (special == "VK_VOLUME_UP") vk = VK_VOLUME_UP;
            else if (special == "VK_VOLUME_DOWN") vk = VK_VOLUME_DOWN;
            else if (special == "VK_VOLUME_MUTE") vk = VK_VOLUME_MUTE;
        } else {
            vk = VkKeyScanA(keys[i]) & 0xFF;
        }

        if (vk == 0) continue;

        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = vk;
        count++;

        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = vk;
        inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        count++;
    }

    if (count > 0) {
        SendInput(count, inputs, sizeof(INPUT));
    }
    return {true, "Keys sent: " + keys, {}};
}

ActionResult WinAPIExecutor::run_cmd(const std::string& command, int timeout_sec) {
    if (command.empty()) return {false, "No command specified", {}};

    std::string escaped = command;
    for (size_t i = 0; i < escaped.size(); i++) {
        if (escaped[i] == '"') { escaped.insert(i, 1, '"'); i++; }
    }
    std::string full_cmd = "cmd.exe /S /C \"" + escaped + "\"";
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hstdout_r, hstdout_w;
    CreatePipe(&hstdout_r, &hstdout_w, &sa, 0);
    SetHandleInformation(hstdout_r, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hstdout_w;
    si.hStdError  = hstdout_w;
    PROCESS_INFORMATION pi;

    char cmd_buf[8192];
    strncpy_s(cmd_buf, full_cmd.c_str(), _TRUNCATE);

    // Always run from user Desktop so relative paths work correctly
    const char* desktop_dir = "C:\\Users\\utkarsh_kumar\\Desktop";
    if (!CreateProcessA(nullptr, cmd_buf, nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, desktop_dir, &si, &pi)) {
        CloseHandle(hstdout_r);
        CloseHandle(hstdout_w);
        return {false, "Failed to start CMD", {}};
    }

    std::string output;
    std::thread reader([&]() {
        char buf[4096];
        DWORD read;
        while (ReadFile(hstdout_r, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
            buf[read] = 0;
            output += buf;
        }
    });

    WaitForSingleObject(pi.hProcess, timeout_sec * 1000);
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(hstdout_w);
    reader.join();
    CloseHandle(hstdout_r);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    Logger::instance().info("CMD executed (" + std::to_string(output.size()) + " bytes)");
    return {true, "Command executed", {{"output", output}}};
}

static const char b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

ActionResult WinAPIExecutor::run_powershell(const std::string& command, int timeout_sec) {
    if (command.empty()) return {false, "No command specified", {}};

    // Run powershell with Desktop as working directory so relative paths work
    std::string full_cmd = "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"" + command + "\"";
    return run_cmd(full_cmd, timeout_sec);
}

ActionResult WinAPIExecutor::notify(const std::string& title, const std::string& message) {
    static HWND hwnd = nullptr;
    static ATOM atom = 0;
    if (!atom) {
        WNDCLASSA wc = {0};
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "SARA_Notify";
        atom = RegisterClassA(&wc);
        hwnd = CreateWindowExA(0, "SARA_Notify", "", WS_POPUP, 0, 0, 0, 0,
            nullptr, nullptr, wc.hInstance, nullptr);
    }

    NOTIFYICONDATAA nid = { sizeof(nid) };
    nid.hWnd = hwnd;
    nid.uID = 2;
    nid.uFlags = NIF_INFO | NIF_ICON | NIF_TIP;
    nid.dwInfoFlags = NIIF_INFO;
    nid.uTimeout = 8000;
    strncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    strncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    strncpy_s(nid.szTip, "SARA", _TRUNCATE);
    nid.hIcon = LoadIconA(nullptr, MAKEINTRESOURCEA(32512));

    bool ok = Shell_NotifyIconA(NIM_ADD, &nid) != FALSE;
    if (ok) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        Shell_NotifyIconA(NIM_DELETE, &nid);
        return {true, "Notification shown: " + title, {}};
    }

    // Fallback: MessageBox
    MessageBoxA(nullptr, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    return {true, "Notification shown (fallback): " + title, {}};
}

ActionResult WinAPIExecutor::volume_set(int level) {
    HRESULT hr = CoInitialize(NULL);
    bool co_init = SUCCEEDED(hr);

    IMMDeviceEnumerator *deviceEnumerator = NULL;
    static const GUID CLSID_MMDeviceEnum = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
    static const GUID IID_IMMDeviceEnum  = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
    hr = CoCreateInstance(CLSID_MMDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_IMMDeviceEnum, (LPVOID *)&deviceEnumerator);
    if (SUCCEEDED(hr) && deviceEnumerator) {
        IMMDevice *defaultDevice = NULL;
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
        if (SUCCEEDED(hr) && defaultDevice) {
            IAudioEndpointVolume *endpointVolume = NULL;
            // Use IID_IAudioEndpointVolume instead of __uuidof to avoid MinGW compiler bugs
            static const GUID IID_IAudioEndpointVol = {0x5CDF2C82, 0x841E, 0x4546, {0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A}};
            hr = defaultDevice->Activate(IID_IAudioEndpointVol, CLSCTX_INPROC_SERVER, NULL, (LPVOID *)&endpointVolume);
            if (SUCCEEDED(hr) && endpointVolume) {
                float fLevel = (float)level / 100.0f;
                endpointVolume->SetMasterVolumeLevelScalar(fLevel, NULL);
                endpointVolume->Release();
            }
            defaultDevice->Release();
        }
        deviceEnumerator->Release();
    }
    
    if (co_init) CoUninitialize();

    return {true, "Volume set to " + std::to_string(level) + "%", {}};
}

ActionResult WinAPIExecutor::volume_mute(bool mute) {
    HRESULT hr = CoInitialize(NULL);
    bool co_init = SUCCEEDED(hr);
    IMMDeviceEnumerator *deviceEnumerator = NULL;
    static const GUID CLSID_MMDeviceEnum = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
    static const GUID IID_IMMDeviceEnum  = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
    hr = CoCreateInstance(CLSID_MMDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_IMMDeviceEnum, (LPVOID *)&deviceEnumerator);
    if (SUCCEEDED(hr) && deviceEnumerator) {
        IMMDevice *defaultDevice = NULL;
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
        if (SUCCEEDED(hr) && defaultDevice) {
            IAudioEndpointVolume *endpointVolume = NULL;
            static const GUID IID_IAudioEndpointVol = {0x5CDF2C82, 0x841E, 0x4546, {0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A}};
            hr = defaultDevice->Activate(IID_IAudioEndpointVol, CLSCTX_INPROC_SERVER, NULL, (LPVOID *)&endpointVolume);
            if (SUCCEEDED(hr) && endpointVolume) {
                endpointVolume->SetMute(mute ? TRUE : FALSE, NULL);
                endpointVolume->Release();
            }
            defaultDevice->Release();
        }
        deviceEnumerator->Release();
    }
    if (co_init) CoUninitialize();
    return {true, mute ? "Muted" : "Unmuted", {}};
}

ActionResult WinAPIExecutor::clipboard_write(const std::string& text) {
    if (!OpenClipboard(nullptr)) return {false, "Failed to open clipboard", {}};
    EmptyClipboard();

    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!hg) { CloseClipboard(); return {false, "Failed to allocate clipboard memory", {}}; }

    memcpy(GlobalLock(hg), text.c_str(), text.size() + 1);
    GlobalUnlock(hg);
    SetClipboardData(CF_TEXT, hg);
    CloseClipboard();

    return {true, "Clipboard written", {}};
}

ActionResult WinAPIExecutor::clipboard_read() {
    if (!OpenClipboard(nullptr)) return {false, "Failed to open clipboard", {}};

    HANDLE hg = GetClipboardData(CF_TEXT);
    if (!hg) { CloseClipboard(); return {false, "No text on clipboard", {}}; }

    char* data = static_cast<char*>(GlobalLock(hg));
    std::string text = data ? data : "";
    GlobalUnlock(hg);
    CloseClipboard();

    return {true, "Clipboard read", {{"text", text}}};
}

ActionResult WinAPIExecutor::get_ip_address() {
    ULONG buf_len = 15000;
    IP_ADAPTER_ADDRESSES* addrs = (IP_ADAPTER_ADDRESSES*)malloc(buf_len);
    if (!addrs) return {false, "Memory allocation failed", {}};

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    DWORD ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &buf_len);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        free(addrs);
        addrs = (IP_ADAPTER_ADDRESSES*)malloc(buf_len);
        if (!addrs) return {false, "Memory allocation failed", {}};
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &buf_len);
    }

    if (ret != NO_ERROR) {
        free(addrs);
        return {false, "Failed to get network adapters", {}};
    }

    json list = json::array();
    for (IP_ADAPTER_ADDRESSES* a = addrs; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (IP_ADAPTER_UNICAST_ADDRESS* u = a->FirstUnicastAddress; u; u = u->Next) {
            sockaddr_in* sa = (sockaddr_in*)u->Address.lpSockaddr;
            char ip[46];
            inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
            int wlen = WideCharToMultiByte(CP_UTF8, 0, a->Description, -1, nullptr, 0, nullptr, nullptr);
            std::string desc(wlen - 1, 0);
            if (wlen > 1) WideCharToMultiByte(CP_UTF8, 0, a->Description, -1, &desc[0], wlen, nullptr, nullptr);
            json entry;
            entry["adapter"] = desc;
            entry["ip"] = ip;
            list.push_back(entry);
        }
    }
    free(addrs);

    if (list.empty()) {
        return {true, "IP addresses: none found (disconnected?)", list};
    }
    return {true, "IP addresses retrieved", list};
}

// ─────────────────────────────────────────────────────────────────────────────
// New action implementations
// ─────────────────────────────────────────────────────────────────────────────

ActionResult WinAPIExecutor::play_youtube(const std::string& query) {
    if (query.empty()) return {false, "No search query provided", {}};
    
    // Fetch YouTube search, extract first video ID, and play it.
    // If it fails, fallback to opening the search page.
    std::string ps = 
        "$q = [uri]::EscapeDataString('" + query + "'); "
        "$url = 'https://www.youtube.com/results?search_query=' + $q; "
        "try { "
        "  $html = (Invoke-WebRequest -Uri $url -UseBasicParsing).Content; "
        "  if ($html -match 'watch\\?v=([a-zA-Z0-9_-]{11})') { "
        "    Start-Process ('https://www.youtube.com/watch?v=' + $Matches[1]); "
        "  } else { "
        "    Start-Process $url; "
        "  } "
        "} catch { "
        "  Start-Process $url; "
        "}";
        
    auto res = run_powershell(ps, 15);
    if (!res.success) {
        // Ultimate fallback
        std::string fallback = "https://www.youtube.com/results?search_query=";
        for (char c : query) {
            if (std::isalnum((unsigned char)c)) fallback += c;
            else if (c == ' ') fallback += '+';
            else { char buf[4]; sprintf_s(buf, "%%%02X", (unsigned char)c); fallback += buf; }
        }
        ShellExecuteA(nullptr, "open", fallback.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return {true, "Opened YouTube search fallback for: " + query, {{"url", fallback}}};
    }
    
    Logger::instance().info("play_youtube: Opened video for '" + query + "'");
    return {true, "Playing YouTube result for: " + query, {}};
}

ActionResult WinAPIExecutor::search_google(const std::string& query) {
    // Legacy shim — now routes to the search plugin instead of opening Google.
    // The actual formatting and Telegram response is handled by NativeCommandRouter::handle_search.
    // Here we just run the plugin and return the raw JSON answer for any caller that needs it.
    if (query.empty()) return {false, "No search query provided", {}};

    // Build JSON request for the plugin
    std::string json_input =
        "{\"plugin\":\"search\",\"action\":\"search\",\"query\":\"" + query +
        "\",\"scrape\":false,\"max_urls\":5}";

    // Locate search_plugin.exe
    char exe_path_buf[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path_buf, MAX_PATH);
    std::string exe_dir(exe_path_buf);
    auto slash = exe_dir.find_last_of("\\/");
    if (slash != std::string::npos) exe_dir = exe_dir.substr(0, slash);

    std::string plugin_exe = exe_dir + "\\search_plugin.exe";
    if (!std::filesystem::exists(plugin_exe))
        plugin_exe = exe_dir + "\\plugins\\search_plugin\\search_plugin.exe";
    if (!std::filesystem::exists(plugin_exe))
        plugin_exe = "C:\\Users\\utkarsh_kumar\\Desktop\\sara\\plugins\\search_plugin\\search_plugin.exe";
    if (!std::filesystem::exists(plugin_exe)) {
        // Hard fallback: open browser if plugin missing
        std::string url = "https://www.google.com/search?q=";
        for (char c : query) {
            if (std::isalnum((unsigned char)c)) url += c;
            else if (c == ' ') url += '+';
            else { char buf[4]; sprintf_s(buf, "%%%02X", (unsigned char)c); url += buf; }
        }
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return {true, "Opened browser search (plugin not found): " + query, {{"url", url}}};
    }

    // Use pipes to send JSON via stdin and read output
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE h_out_read = NULL, h_out_write = NULL;
    if (!CreatePipe(&h_out_read, &h_out_write, &sa, 0))
        return {false, "Search pipe failed", {}};
    SetHandleInformation(h_out_read, HANDLE_FLAG_INHERIT, 0);

    HANDLE h_in_read = NULL, h_in_write = NULL;
    if (!CreatePipe(&h_in_read, &h_in_write, &sa, 0)) {
        CloseHandle(h_out_read); CloseHandle(h_out_write);
        return {false, "Search pipe failed", {}};
    }
    SetHandleInformation(h_in_write, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = h_out_write;
    si.hStdError  = h_out_write;
    si.hStdInput  = h_in_read;

    PROCESS_INFORMATION pi = {};
    std::string cmd_line = "\"" + plugin_exe + "\"";
    std::vector<char> cmd_buf(cmd_line.begin(), cmd_line.end());
    cmd_buf.push_back('\0');

    bool created = CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    CloseHandle(h_out_write);
    CloseHandle(h_in_read);

    if (!created) {
        CloseHandle(h_out_read); CloseHandle(h_in_write);
        return {false, "Failed to start search plugin", {}};
    }

    DWORD written = 0;
    WriteFile(h_in_write, json_input.c_str(), (DWORD)json_input.size(), &written, NULL);
    CloseHandle(h_in_write);

    std::string output;
    char buf[4096];
    DWORD bytes_read = 0;
    while (ReadFile(h_out_read, buf, sizeof(buf) - 1, &bytes_read, NULL) && bytes_read > 0) {
        buf[bytes_read] = '\0';
        output += buf;
    }
    WaitForSingleObject(pi.hProcess, 20000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(h_out_read);

    if (output.empty()) return {false, "Search plugin returned no output", {}};

    try {
        auto j = nlohmann::json::parse(output);
        if (!j.value("success", false))
            return {false, "Search failed: " + j.value("error", "unknown"), {}};
        std::string answer = j.value("answer", "");
        return {true, answer.empty() ? "Search complete: " + query : answer, j};
    } catch (...) {
        return {false, "Search plugin parse error", {}};
    }
}

ActionResult WinAPIExecutor::open_website(const std::string& url) {
    if (url.empty()) return {false, "No URL provided", {}};
    HINSTANCE res = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)res <= 32) return {false, "Failed to open URL: " + url, {}};
    return {true, "Opened: " + url, {}};
}

ActionResult WinAPIExecutor::change_brightness(int level) {
    level = std::max(0, std::min(100, level));
    // Use PowerShell WMI to set brightness — most reliable method
    std::string ps = "(Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightnessMethods).WmiSetBrightness(1,"
        + std::to_string(level) + ")";
    auto result = run_powershell(ps, 10);
    if (result.success) {
        Logger::instance().info("Brightness set to " + std::to_string(level) + "%");
        return {true, "Brightness set to " + std::to_string(level) + "%", {}};
    }
    return {false, "Failed to set brightness (laptop display required)", {}};
}

ActionResult WinAPIExecutor::lock_pc() {
    if (LockWorkStation()) return {true, "PC locked", {}};
    return {false, "Failed to lock PC", {}};
}

ActionResult WinAPIExecutor::write_file(const std::string& path, const std::string& content) {
    if (path.empty()) return {false, "No path provided", {}};
    
    // Convert relative path to absolute if needed
    std::string final_path = path;
    if (path.find(":") == std::string::npos && path.find("\\") != 0 && path.find("/") != 0) {
        final_path = "C:\\Users\\utkarsh_kumar\\Desktop\\" + path;
    }
    
    auto pos = final_path.find_last_of("\\/");
    if (pos != std::string::npos) {
        std::string dir = final_path.substr(0, pos);
        std::string cmd = "cmd.exe /c mkdir \"" + dir + "\" 2>nul";
        system(cmd.c_str());
    }

    FILE* f;
    fopen_s(&f, final_path.c_str(), "wb");
    if (!f) return {false, "Failed to open file for writing", {}};
    fwrite(content.c_str(), 1, content.size(), f);
    fclose(f);
    return {true, "File written successfully", {{"path", final_path}}};
}

ActionResult WinAPIExecutor::read_file(const std::string& path) {
    if (path.empty()) return {false, "No path provided", {}};
    std::string final_path = path;
    if (path.find(":") == std::string::npos && path.find("\\") != 0 && path.find("/") != 0) {
        final_path = "C:\\Users\\utkarsh_kumar\\Desktop\\" + path;
    }
    
    FILE* f;
    fopen_s(&f, final_path.c_str(), "rb");
    if (!f) return {false, "Failed to open file for reading", {}};
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    std::string content(size, '\0');
    fread(&content[0], 1, size, f);
    fclose(f);
    
    return {true, "File read successfully", {{"content", content}}};
}

ActionResult WinAPIExecutor::list_dir(const std::string& path) {
    if (path.empty()) return {false, "No path provided", {}};
    std::string final_path = path;
    if (path.find(":") == std::string::npos && path.find("\\") != 0 && path.find("/") != 0) {
        final_path = "C:\\Users\\utkarsh_kumar\\Desktop\\" + path;
    }
    
    std::string cmd = "powershell -NoProfile -NonInteractive -Command \"Get-ChildItem '" + final_path + "' | Select-Object Name, Length, LastWriteTime | ConvertTo-Json -Compress\"";
    return run_cmd(cmd, 10);
}

} // namespace sara

