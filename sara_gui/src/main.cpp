#include <windows.h>
#include <d3d11.h>
#include <string>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdio>
#include <vector>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>
#include "../include/IPCClient.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

using json = nlohmann::json;
using namespace sara;

static IPCClient g_ipc;
static json g_cfg;
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HWND g_hwnd = nullptr;
static WNDCLASSEXW g_wc = {};
static std::string g_log_buf;
static char g_token[4096] = {};
static char g_password[4096] = "test1234";
static std::atomic<bool> g_polling{false};
static std::thread g_poll_thread;
static std::mutex g_log_mutex;
static std::vector<std::string> g_pending_logs;
static long long g_last_update_id = 0;
static std::string g_settings_path;
static std::string g_exe_dir;
static std::string g_bin_dir;
static char g_root_password[4096] = {};
static std::mutex g_users_mutex;
struct GuiUser { long long user_id = 0; std::string username, first_name, last_name, added_at; bool blocked = false; };
static std::vector<GuiUser> g_users;

static void log(const std::string& msg) {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm; localtime_s(&tm, &t);
    char ts[32]; strftime(ts, sizeof(ts), "%H:%M:%S", &tm);
    g_log_buf += std::string(ts) + " " + msg + "\n";
    if (g_log_buf.size() > 16384) g_log_buf.erase(0, g_log_buf.size() - 8192);
}

static void log_threadsafe(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_pending_logs.push_back(msg);
}

static void flush_pending_logs() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    for (auto& m : g_pending_logs) log(m);
    g_pending_logs.clear();
}

static void load_cfg() {
    std::ifstream f(g_settings_path);
    if (f.is_open()) { f >> g_cfg; f.close(); }
    else g_cfg = json::object();
}

static void save_cfg() {
    std::ofstream f(g_settings_path);
    if (f.is_open()) { f << g_cfg.dump(2); f.close(); }
}

static void ui_from_cfg() {
    auto tg = g_cfg.value("telegram", json::object());
    snprintf(g_token, sizeof(g_token), "%s", tg.value("token", "").c_str());
    snprintf(g_password, sizeof(g_password), "%s", tg.value("password", "test123f").c_str());
    
}

static void cfg_from_ui() {
    g_cfg["telegram"]["token"] = g_token;
    g_cfg["telegram"]["password"] = g_password;
    
}

static void poll_telegram() {
    IPCClient poll_ipc;
    if (!poll_ipc.connect()) return;
    while (g_polling) {
        json msg;
        msg["type"] = "get_telegram_updates";
        msg["request_id"] = "poll";
        msg["payload"] = json::object();
        auto resp = poll_ipc.send_message(msg);
        if (resp.contains("payload") && resp["payload"].contains("messages")) {
            for (auto& m : resp["payload"]["messages"]) {
                long long uid = m.value("update_id", 0LL);
                if (uid <= g_last_update_id) continue;
                g_last_update_id = uid;
                std::string from = m.value("from_name", "Unknown");
                std::string text = m.value("text", "");
                log_threadsafe("Telegram from " + from + ": " + text);
            }
        }
        
        json root_msg;
        root_msg["type"] = "command";
        root_msg["request_id"] = "get_root_pw";
        root_msg["payload"] = {{"action", "get_root_password"}};
        auto root_resp = poll_ipc.send_message(root_msg);
        if (root_resp.contains("payload") && root_resp["payload"].contains("password")) {
            std::string pw = root_resp["payload"]["password"].get<std::string>();
            snprintf(g_root_password, sizeof(g_root_password), "%s", pw.c_str());
        }

        // Poll trusted users list
        json users_msg;
        users_msg["type"] = "command";
        users_msg["request_id"] = "get_users";
        users_msg["payload"] = {{"action", "get_users"}};
        auto users_resp = poll_ipc.send_message(users_msg);
        if (users_resp.contains("payload") && users_resp["payload"].contains("users")) {
            std::vector<GuiUser> fresh;
            for (auto& u : users_resp["payload"]["users"]) {
                GuiUser gu;
                gu.user_id    = u.value("user_id",    0LL);
                gu.username   = u.value("username",   "");
                gu.first_name = u.value("first_name", "");
                gu.last_name  = u.value("last_name",  "");
                gu.added_at   = u.value("added_at",   "");
                gu.blocked    = u.value("blocked",    false);
                fresh.push_back(gu);
            }
            std::lock_guard<std::mutex> lk(g_users_mutex);
            g_users = std::move(fresh);
        }

        for (int i = 0; i < 20 && g_polling; i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

static bool launch_agent() {
    std::string path = g_bin_dir + "sara_agent.exe";
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return false;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(path.c_str(), nullptr, nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, g_bin_dir.c_str(), &si, &pi))
        return false;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

static bool auto_connect() {
    if (g_ipc.connect()) {
        log("Connected to sara_agent.exe");
        g_ipc.send_command("reload_config", g_cfg);
        if (!g_polling) { g_polling = true; g_poll_thread = std::thread(poll_telegram); }
        return true;
    }
    log("Agent not found. Launching sara_agent.exe...");
    if (!launch_agent()) {
        log("Failed: sara_agent.exe not found in " + g_exe_dir);
        return false;
    }
    for (int n = 0; n < 15; n++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        if (g_ipc.connect()) {
            log("Connected to sara_agent.exe");
            g_ipc.send_command("reload_config", g_cfg);
            if (!g_polling) { g_polling = true; g_poll_thread = std::thread(poll_telegram); }
            return true;
        }
    }
    log("ERROR: Agent launched but connection timed out");
    return false;
}

static bool create_render_target() {
    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) return false;
    if (FAILED(g_pd3dDevice->CreateRenderTargetView(back_buffer, nullptr, &g_mainRenderTargetView))) {
        back_buffer->Release(); return false;
    }
    back_buffer->Release(); return true;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK wnd_proc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hw, msg, wp, lp)) return 1;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (msg == WM_SIZE && wp != SIZE_MINIMIZED && g_pSwapChain) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
        if (SUCCEEDED(g_pSwapChain->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0)))
            create_render_target();
        ImGui_ImplDX11_CreateDeviceObjects();
    }
    return DefWindowProc(hw, msg, wp, lp);
}

static bool create_device(HWND hw) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0; sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hw;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL level;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &level, &g_pd3dDeviceContext)))
        return false;
    return create_render_target();
}

static void cleanup_device() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int show) {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    g_bin_dir = buf;
    auto pos = g_bin_dir.find_last_of("\\/");
    if (pos != std::string::npos) g_bin_dir = g_bin_dir.substr(0, pos + 1);
    g_exe_dir = g_bin_dir;
    auto parent_pos = g_exe_dir.find_last_of("\\/", g_exe_dir.size() - 2);
    if (parent_pos != std::string::npos) {
        std::string parent_dir = g_exe_dir.substr(0, parent_pos + 1);
        DWORD attr = GetFileAttributesA((parent_dir + "data").c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
            g_exe_dir = parent_dir;
    }
    g_settings_path = g_exe_dir + "settings.json";

    g_wc.cbSize = sizeof(g_wc); g_wc.lpfnWndProc = wnd_proc; g_wc.hInstance = hi;
    g_wc.hCursor = LoadCursor(nullptr, IDC_ARROW); g_wc.lpszClassName = L"SARA_GUI_IMGUI";
    RegisterClassExW(&g_wc);
    g_hwnd = CreateWindowW(L"SARA_GUI_IMGUI", L"SARA Control Panel",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 680, 600, nullptr, nullptr, hi, nullptr);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, show); UpdateWindow(g_hwnd);
    if (!create_device(g_hwnd)) { DestroyWindow(g_hwnd); return 1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    load_cfg(); ui_from_cfg();
    log("SARA Control Panel ready");
    log("Settings: " + g_settings_path);
    std::thread([]() { auto_connect(); }).detach();

    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        if (!running) break;

        flush_pending_logs();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("SARA Control Panel", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::MenuItem("Connect", nullptr, g_ipc.is_connected())) {
                if (g_ipc.is_connected()) {
                    g_ipc.disconnect();
                    log("Disconnected");
                } else {
                    std::thread([]() { auto_connect(); }).detach();
                }
            }
            if (ImGui::MenuItem("Start Polling", nullptr, g_polling)) {
                if (g_polling) {
                    g_polling = false;
                    if (g_poll_thread.joinable()) g_poll_thread.join();
                    log("Polling stopped");
                } else if (g_ipc.is_connected()) {
                    g_polling = true;
                    g_poll_thread = std::thread(poll_telegram);
                    log("Polling started");
                } else log("Connect to agent first");
            }
            if (ImGui::MenuItem("Refresh")) { load_cfg(); ui_from_cfg(); log("Refreshed"); }
            if (ImGui::MenuItem("Quit")) { running = false; }
            ImGui::EndMenuBar();
        }

        ImGui::Text("Agent: %s", g_ipc.is_connected() ? "Connected" : "Disconnected");
        ImGui::SameLine();
        ImGui::Text("  Polling: %s", g_polling ? "ON" : "OFF");
        ImGui::SameLine();
        if (ImGui::SmallButton(g_ipc.is_connected() ? "Disconnect" : "Connect")) {
            if (g_ipc.is_connected()) {
                if (g_polling) { g_polling = false; if (g_poll_thread.joinable()) g_poll_thread.join(); }
                g_ipc.disconnect();
                log("Disconnected");
            } else {
                std::thread([]() { auto_connect(); }).detach();
            }
        }
        ImGui::Separator();

        if (ImGui::BeginTabBar("##MainTabs")) {

            // ── TAB 1: Config ─────────────────────────────────────────────
            if (ImGui::BeginTabItem("Config")) {
                ImGui::Text("Telegram Configuration");
                ImGui::InputText("Bot Token", g_token, sizeof(g_token));
                ImGui::InputText("Session Password", g_password, sizeof(g_password), ImGuiInputTextFlags_Password);

                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Pending Root Password (New Device)");
                ImGui::InputText("##RootPw", g_root_password, sizeof(g_root_password), ImGuiInputTextFlags_ReadOnly);

                if (ImGui::Button("Test Telegram")) {
                    cfg_from_ui(); save_cfg();
                    auto t = g_cfg.value("telegram", json::object()).value("token", "");
                    if (t.empty() || t == "YOUR_TELEGRAM_BOT_TOKEN") { log("ERROR: Set Telegram token first"); }
                    else {
                        log("Testing Telegram...");
                        std::thread([t]() {
                            std::string url = "https://api.telegram.org/bot" + t + "/getMe";
                            std::string cmd = "powershell -Command \"try{$r=Invoke-WebRequest -Uri '"
                                + url + "' -UseBasicParsing;if($r.Content-match'\\\"ok\\\":true'){exit 0}else{exit 1}}catch{exit 2}\"";
                            int r = system(cmd.c_str());
                            if (r == 0) log_threadsafe("Telegram API: Connected!");
                            else if (r == 1) log_threadsafe("Telegram API: Invalid token");
                            else log_threadsafe("Telegram API: Connection failed");
                            if (r == 0 && g_ipc.is_connected()) {
                                g_ipc.send_command("reload_config", g_cfg);
                                log_threadsafe("Agent reloaded with new token");
                            }
                        }).detach();
                    }
                }
                ImGui::Separator();

                ImGui::Text("Native mode");
                ImGui::Separator();
                if (ImGui::Button("Save Config")) {
                    save_cfg(); log("Saved to settings.json");
                    if (g_ipc.is_connected()) {
                        g_ipc.send_command("reload_config", g_cfg);
                        log("Config sent to agent");
                    } else log("Connect to agent to send config.");
                }
                ImGui::EndTabItem();
            }

            // ── TAB 2: Users ──────────────────────────────────────────────
            if (ImGui::BeginTabItem("Users")) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Trusted User Management");
                ImGui::Spacing();

                std::lock_guard<std::mutex> lk(g_users_mutex);
                if (g_users.empty()) {
                    ImGui::TextDisabled("No trusted users yet. A new device connecting will appear here.");
                } else {
                    // Table header
                    if (ImGui::BeginTable("##UsersTable", 6,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                            ImVec2(0, -50))) {
                        ImGui::TableSetupColumn("ID",          ImGuiTableColumnFlags_WidthFixed, 110);
                        ImGui::TableSetupColumn("Name",        ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("@Username",   ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Added At",    ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Status",      ImGuiTableColumnFlags_WidthFixed,  70);
                        ImGui::TableSetupColumn("Action",      ImGuiTableColumnFlags_WidthFixed,  80);
                        ImGui::TableHeadersRow();

                        for (auto& u : g_users) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%lld", u.user_id);

                            ImGui::TableSetColumnIndex(1);
                            std::string full = u.first_name;
                            if (!u.last_name.empty()) full += " " + u.last_name;
                            ImGui::TextUnformatted(full.c_str());

                            ImGui::TableSetColumnIndex(2);
                            ImGui::TextUnformatted(u.username.empty() ? "-" : ("@" + u.username).c_str());

                            ImGui::TableSetColumnIndex(3);
                            // Show only date portion
                            std::string ts = u.added_at.size() >= 10 ? u.added_at.substr(0, 10) : u.added_at;
                            ImGui::TextUnformatted(ts.c_str());

                            ImGui::TableSetColumnIndex(4);
                            if (u.blocked)
                                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "BLOCKED");
                            else
                                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "OK");

                            ImGui::TableSetColumnIndex(5);
                            ImGui::PushID((int)u.user_id);
                            if (u.blocked) {
                                if (ImGui::SmallButton("Unblock")) {
                                    long long uid = u.user_id;
                                    std::thread([uid]() {
                                        json msg;
                                        msg["type"] = "command";
                                        msg["request_id"] = "unblock";
                                        msg["payload"] = {{"action","unblock_user"},{"user_id", uid}};
                                        g_ipc.send_message(msg);
                                        log_threadsafe("Unblocked user " + std::to_string(uid));
                                    }).detach();
                                }
                            } else {
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f,0.15f,0.15f,1.0f));
                                if (ImGui::SmallButton("Block")) {
                                    long long uid = u.user_id;
                                    std::thread([uid]() {
                                        json msg;
                                        msg["type"] = "command";
                                        msg["request_id"] = "block";
                                        msg["payload"] = {{"action","block_user"},{"user_id", uid}};
                                        g_ipc.send_message(msg);
                                        log_threadsafe("Blocked user " + std::to_string(uid));
                                    }).detach();
                                }
                                ImGui::PopStyleColor();
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }
                }
                ImGui::EndTabItem();
            }

            // ── TAB 3: Log ────────────────────────────────────────────────
            if (ImGui::BeginTabItem("Log")) {
                if (ImGui::SmallButton("Clear")) { g_log_buf.clear(); }
                ImGui::BeginChild("LogScrolling", ImVec2(0, 0), true);
                ImGui::TextUnformatted(g_log_buf.c_str());
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();


        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        ImVec4 c = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&c);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    g_polling = false;
    if (g_poll_thread.joinable()) g_poll_thread.join();
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    cleanup_device(); g_ipc.disconnect();
    DestroyWindow(g_hwnd); return 0;
}
