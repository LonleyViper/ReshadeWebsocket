#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#define WIN32_LEAN_AND_MEAN  // Prevent windows.h from including winsock.h
#include <winsock2.h>        // Include winsock2.h FIRST
#include <ws2tcpip.h>        // Include ws2tcpip.h second
#include <windows.h>         // Now include windows.h
#include <imgui.h>
#include <reshade.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>
#include <sstream>
#include <memory>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

// Addon info
constexpr auto ADDON_NAME = "StreamerbotControl";
constexpr auto ADDON_VERSION = "1.1";
constexpr auto ADDON_AUTHOR = "LonelyViper";
constexpr auto ADDON_COPYRIGHT = "Copyright (c) 2025 LonelyLabs";
constexpr auto ADDON_DESCRIPTION = "Control ReShade effects via TCP commands from Streamerbot with auto-restart";
constexpr auto BUILD_DATE = __DATE__ " " __TIME__;
constexpr int DEFAULT_PORT = 7777;
constexpr size_t BUFFER_SIZE = 1024;
constexpr size_t MAX_LOG_ENTRIES = 100;

struct LogEntry {
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    ImVec4 color;
};

struct AddonState {
    // Network state
    std::atomic<bool> server_running{ false };
    std::atomic<bool> should_be_running{ false };  // NEW: Track intended states
    std::unique_ptr<std::thread> server_thread;
    std::unique_ptr<std::thread> monitor_thread;   // NEW: Monitoring thread
    SOCKET server_socket = INVALID_SOCKET;
    int port = DEFAULT_PORT;
    std::mutex socket_mutex;

    // Connection state
    std::atomic<bool> client_connected{ false };
    std::string client_address = "None";
    std::atomic<int> commands_received{ 0 };
    std::atomic<int> restart_count{ 0 };          // NEW: Track restart attempts.

    // Auto-restart settings
    bool auto_restart_enabled = true;             // NEW: Enable/disable auto-restart
    int restart_delay_seconds = 5;               // NEW: Delay between restart attempts
    int max_restart_attempts = 10;               // NEW: Max consecutive restart attempts
    std::chrono::steady_clock::time_point last_restart_attempt; // NEW: Track last restart
    std::chrono::steady_clock::time_point last_successful_start; // NEW: Track successful starts

    // Health monitoring
    std::chrono::steady_clock::time_point last_heartbeat;       // NEW: Track server health
    std::atomic<bool> server_healthy{ true };    // NEW: Server health status

    // ReShade state
    reshade::api::effect_runtime* current_runtime{ nullptr };
    std::vector<std::string> available_techniques;

    // UI state
    std::vector<LogEntry> log_entries;
    std::mutex log_mutex;
    bool show_technique_list = false;
    bool auto_scroll_log = true;
    char port_buffer[16] = "7777";
    bool show_advanced_settings = false;          // NEW: Show advanced restart settings

    // Command processing
    std::chrono::steady_clock::time_point last_command_time;
};

static std::unique_ptr<AddonState> g_state;

// Add log entry
void AddLog(const std::string& message, const ImVec4& color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f)) {
    if (!g_state) return;

    std::lock_guard<std::mutex> lock(g_state->log_mutex);

    LogEntry entry;
    entry.message = message;
    entry.timestamp = std::chrono::system_clock::now();
    entry.color = color;

    g_state->log_entries.push_back(entry);

    // Keep log size reasonable
    if (g_state->log_entries.size() > MAX_LOG_ENTRIES) {
        g_state->log_entries.erase(g_state->log_entries.begin());
    }
}

// Update available techniques
void UpdateAvailableTechniques(reshade::api::effect_runtime* runtime) {
    if (!runtime || !g_state) return;

    g_state->available_techniques.clear();

    runtime->enumerate_techniques(nullptr, [](reshade::api::effect_runtime* rt, reshade::api::effect_technique technique) {
        char tech_name[256] = {};
        rt->get_technique_name(technique, tech_name);
        g_state->available_techniques.push_back(tech_name);
        });

    AddLog("Updated available techniques: " + std::to_string(g_state->available_techniques.size()) + " found",
        ImVec4(0.7f, 0.7f, 1.0f, 1.0f));
}

// Process command
void ProcessCommand(const std::string& command) {
    if (!g_state || !g_state->current_runtime) {
        AddLog("Error: No runtime available", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        return;
    }

    g_state->last_command_time = std::chrono::steady_clock::now();
    g_state->commands_received++;

    // Parse command: FORMAT: "TOGGLE <technique_name>" or "ENABLE <technique_name>" or "DISABLE <technique_name>"
    std::istringstream iss(command);
    std::string action, technique_name;

    iss >> action;
    std::getline(iss >> std::ws, technique_name); // Get rest of line as technique name

    // Convert action to uppercase
    std::transform(action.begin(), action.end(), action.begin(), ::toupper);

    if (technique_name.empty()) {
        AddLog("Error: No technique specified in command", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        return;
    }

    bool found = false;

    g_state->current_runtime->enumerate_techniques(nullptr, [&](reshade::api::effect_runtime* rt, reshade::api::effect_technique technique) {
        char tech_name[256] = {};
        rt->get_technique_name(technique, tech_name);
        std::string name(tech_name);

        // Check for exact match or partial match
        bool matches = (name == technique_name);
        if (!matches) {
            std::string name_lower = name;
            std::string search_lower = technique_name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
            matches = (name_lower.find(search_lower) != std::string::npos);
        }

        if (matches) {
            found = true;
            bool current_state = rt->get_technique_state(technique);
            bool new_state = current_state;

            if (action == "TOGGLE") {
                new_state = !current_state;
            }
            else if (action == "ENABLE" || action == "ON") {
                new_state = true;
            }
            else if (action == "DISABLE" || action == "OFF") {
                new_state = false;
            }
            else {
                AddLog("Unknown action: " + action, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
                return;
            }

            rt->set_technique_state(technique, new_state);

            std::string state_str = new_state ? "ON" : "OFF";
            AddLog("Set " + name + " to " + state_str, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        }
        });

    if (!found) {
        AddLog("Technique not found: " + technique_name, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
    }
}

// NEW: Clean shutdown of server
void CleanShutdownServer() {
    if (g_state->server_socket != INVALID_SOCKET) {
        closesocket(g_state->server_socket);
        g_state->server_socket = INVALID_SOCKET;
    }
    g_state->server_running = false;
    g_state->server_healthy = false;
    g_state->client_connected = false;
    g_state->client_address = "None";
}

// TCP Server thread
void ServerThread() {
    AddLog("Server thread started", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        AddLog("WSAStartup failed", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        g_state->server_healthy = false;
        return;
    }

    // Create socket
    g_state->server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_state->server_socket == INVALID_SOCKET) {
        AddLog("Socket creation failed", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        WSACleanup();
        g_state->server_healthy = false;
        return;
    }

    // Set socket options
    int opt = 1;
    setsockopt(g_state->server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    // Set non-blocking mode
    u_long mode = 1;
    ioctlsocket(g_state->server_socket, FIONBIO, &mode);

    // Bind socket
    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(g_state->port);

    if (bind(g_state->server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        AddLog("Bind failed on port " + std::to_string(g_state->port), ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        CleanShutdownServer();
        WSACleanup();
        return;
    }

    // Listen for connections
    if (listen(g_state->server_socket, SOMAXCONN) == SOCKET_ERROR) {
        AddLog("Listen failed", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        CleanShutdownServer();
        WSACleanup();
        return;
    }

    AddLog("Server listening on port " + std::to_string(g_state->port), ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

    // Mark server as healthy and update timestamps
    g_state->server_healthy = true;
    g_state->last_heartbeat = std::chrono::steady_clock::now();
    g_state->last_successful_start = std::chrono::steady_clock::now();
    g_state->restart_count = 0; // Reset restart count on successful start

    while (g_state->server_running && g_state->should_be_running) {
        // Update heartbeat
        g_state->last_heartbeat = std::chrono::steady_clock::now();

        // Accept connections
        sockaddr_in client_addr = {};
        int client_len = sizeof(client_addr);
        SOCKET client_socket = accept(g_state->server_socket, (sockaddr*)&client_addr, &client_len);

        if (client_socket != INVALID_SOCKET) {
            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, INET_ADDRSTRLEN);
            g_state->client_address = addr_str;
            g_state->client_connected = true;

            AddLog("Client connected from " + g_state->client_address, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));

            // Handle client
            char buffer[BUFFER_SIZE];
            while (g_state->server_running && g_state->should_be_running) {
                // Update heartbeat during active connection
                g_state->last_heartbeat = std::chrono::steady_clock::now();

                int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

                if (bytes_received > 0) {
                    buffer[bytes_received] = '\0';
                    std::string command(buffer);

                    // Remove newlines/carriage returns
                    command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());
                    command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());

                    AddLog("Received: " + command, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                    ProcessCommand(command);

                    // Send acknowledgment
                    std::string response = "OK\n";
                    send(client_socket, response.c_str(), (int)response.length(), 0);
                }
                else if (bytes_received == 0) {
                    // Client disconnected gracefully
                    break;
                }
                else {
                    // Check for actual error (not just non-blocking)
                    int error = WSAGetLastError();
                    if (error != WSAEWOULDBLOCK) {
                        AddLog("Socket error: " + std::to_string(error), ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
                        break;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            g_state->client_connected = false;
            g_state->client_address = "None";
            closesocket(client_socket);
            AddLog("Client disconnected", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        }
        else {
            // Check for socket errors that would indicate server failure
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK && error != WSAECONNABORTED) {
                AddLog("Accept error: " + std::to_string(error), ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                g_state->server_healthy = false;
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    CleanShutdownServer();
    WSACleanup();
    AddLog("Server stopped", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
}

// NEW: Server monitor and auto-restart thread
void MonitorThread() {
    AddLog("Monitor thread started", ImVec4(0.0f, 0.8f, 0.8f, 1.0f));

    while (g_state->should_be_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check if auto-restart is enabled
        if (!g_state->auto_restart_enabled) {
            continue;
        }

        // Check if server should be running but isn't
        if (g_state->should_be_running && !g_state->server_running) {
            auto now = std::chrono::steady_clock::now();
            auto time_since_last_restart = std::chrono::duration_cast<std::chrono::seconds>(
                now - g_state->last_restart_attempt).count();

            // Check if enough time has passed since last restart attempt
            if (time_since_last_restart >= g_state->restart_delay_seconds) {
                // Check if we haven't exceeded max restart attempts
                if (g_state->restart_count < g_state->max_restart_attempts) {
                    g_state->restart_count++;
                    g_state->last_restart_attempt = now;

                    AddLog("Auto-restarting server (attempt " + std::to_string(g_state->restart_count) +
                        "/" + std::to_string(g_state->max_restart_attempts) + ")",
                        ImVec4(1.0f, 1.0f, 0.0f, 1.0f));

                    // Stop existing thread if it's still running
                    if (g_state->server_thread && g_state->server_thread->joinable()) {
                        g_state->server_thread->join();
                    }

                    // Start new server thread
                    g_state->server_running = true;
                    g_state->server_thread = std::make_unique<std::thread>(ServerThread);
                }
                else {
                    AddLog("Max restart attempts reached. Auto-restart disabled.", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                    g_state->auto_restart_enabled = false;
                }
            }
        }

        // Check server health (heartbeat)
        if (g_state->server_running) {
            auto now = std::chrono::steady_clock::now();
            auto time_since_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(
                now - g_state->last_heartbeat).count();

            // If no heartbeat for more than 30 seconds, consider server dead
            if (time_since_heartbeat > 30) {
                AddLog("Server heartbeat timeout - marking as unhealthy", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
                g_state->server_healthy = false;

                // Force stop the server to trigger restart
                CleanShutdownServer();
                if (g_state->server_thread && g_state->server_thread->joinable()) {
                    g_state->server_thread->join();
                }
            }
        }
    }

    AddLog("Monitor thread stopped", ImVec4(0.0f, 0.8f, 0.8f, 1.0f));
}

// Start server
void StartServer() {
    if (!g_state) return;

    g_state->port = std::stoi(g_state->port_buffer);
    g_state->should_be_running = true;
    g_state->server_running = true;
    g_state->restart_count = 0; // Reset restart count when manually started

    // Start server thread
    g_state->server_thread = std::make_unique<std::thread>(ServerThread);

    // Start monitor thread if not already running
    if (!g_state->monitor_thread) {
        g_state->monitor_thread = std::make_unique<std::thread>(MonitorThread);
    }
}

// Stop server
void StopServer() {
    if (!g_state) return;

    g_state->should_be_running = false;
    g_state->server_running = false;

    // Close socket to unblock accept()
    if (g_state->server_socket != INVALID_SOCKET) {
        closesocket(g_state->server_socket);
    }

    if (g_state->server_thread && g_state->server_thread->joinable()) {
        g_state->server_thread->join();
    }

    if (g_state->monitor_thread && g_state->monitor_thread->joinable()) {
        g_state->monitor_thread->join();
    }
}

// NEW: Manual restart function
void RestartServer() {
    if (!g_state) return;

    AddLog("Manual server restart requested", ImVec4(0.0f, 1.0f, 1.0f, 1.0f));

    // Stop server
    g_state->server_running = false;
    if (g_state->server_socket != INVALID_SOCKET) {
        closesocket(g_state->server_socket);
    }
    if (g_state->server_thread && g_state->server_thread->joinable()) {
        g_state->server_thread->join();
    }

    // Reset restart count for manual restart
    g_state->restart_count = 0;

    // Start server again
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Brief pause
    g_state->server_running = true;
    g_state->server_thread = std::make_unique<std::thread>(ServerThread);
}

// GUI
static void OnDrawSettings(reshade::api::effect_runtime* runtime) {
    ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), "%s v%s", ADDON_NAME, ADDON_VERSION);
    ImGui::Text("By %s", ADDON_AUTHOR);
    ImGui::Separator();

    // Server status with health indicator
    ImGui::Text("Server Status: %s", g_state->server_running.load() ? "Running" : "Stopped");
    ImGui::SameLine();
    if (g_state->server_running) {
        ImGui::TextColored(g_state->server_healthy ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
            g_state->server_healthy ? "(Healthy)" : "(Unhealthy)");
    }

    ImGui::Text("Client: %s", g_state->client_connected.load() ? g_state->client_address.c_str() : "None");
    ImGui::Text("Commands Received: %d", g_state->commands_received.load());

    // NEW: Auto-restart status
    if (g_state->restart_count > 0) {
        ImGui::Text("Restart Count: %d/%d", g_state->restart_count.load(), g_state->max_restart_attempts);
    }

    ImGui::Separator();

    ImGui::InputText("Port", g_state->port_buffer, sizeof(g_state->port_buffer));

    if (!g_state->server_running) {
        if (ImGui::Button("Start Server")) {
            StartServer();
        }
    }
    else {
        if (ImGui::Button("Stop Server")) {
            StopServer();
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart Server")) {
            RestartServer();
        }
    }

    // NEW: Auto-restart controls
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Auto-Restart Settings:");

    if (ImGui::Checkbox("Enable Auto-Restart", &g_state->auto_restart_enabled)) {
        if (g_state->auto_restart_enabled) {
            g_state->restart_count = 0; // Reset count when re-enabling
            AddLog("Auto-restart enabled", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        }
        else {
            AddLog("Auto-restart disabled", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        }
    }

    if (ImGui::Button("Show Advanced Settings")) {
        g_state->show_advanced_settings = !g_state->show_advanced_settings;
    }

    if (g_state->show_advanced_settings) {
        ImGui::Indent();
        ImGui::SliderInt("Restart Delay (seconds)", &g_state->restart_delay_seconds, 1, 60);
        ImGui::SliderInt("Max Restart Attempts", &g_state->max_restart_attempts, 1, 50);
        if (ImGui::Button("Reset Restart Counter")) {
            g_state->restart_count = 0;
            AddLog("Restart counter reset", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        }
        ImGui::Unindent();
    }

    ImGui::Separator();

    // Available techniques
    if (ImGui::Button("Show Available Techniques")) {
        UpdateAvailableTechniques(runtime);
        g_state->show_technique_list = !g_state->show_technique_list;
    }

    if (g_state->show_technique_list) {
        ImGui::BeginChild("TechniqueList", ImVec2(0, 150), true);
        for (const auto& tech : g_state->available_techniques) {
            if (ImGui::Selectable(tech.c_str())) {
                ImGui::SetClipboardText(tech.c_str());
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to copy");
            }
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
    ImGui::Text("Command Format: <ACTION> <technique_name>");
    ImGui::Text("Actions: TOGGLE, ENABLE/ON, DISABLE/OFF");
    ImGui::Text("Example: TOGGLE MotionBlur");

    ImGui::Separator();

    // Log window
    ImGui::Text("Activity Log:");
    ImGui::Checkbox("Auto-scroll", &g_state->auto_scroll_log);

    ImGui::BeginChild("LogWindow", ImVec2(0, 200), true);

    std::lock_guard<std::mutex> lock(g_state->log_mutex);
    for (const auto& entry : g_state->log_entries) {
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        struct tm timeinfo = {};
        localtime_s(&timeinfo, &time_t);
        char time_buffer[32];
        strftime(time_buffer, sizeof(time_buffer), "[%H:%M:%S]", &timeinfo);

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", time_buffer);
        ImGui::SameLine();
        ImGui::TextColored(entry.color, "%s", entry.message.c_str());
    }

    if (g_state->auto_scroll_log && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

// ReShade callbacks
static void OnInitEffectRuntime(reshade::api::effect_runtime* runtime) {
    if (!g_state) return;
    g_state->current_runtime = runtime;
    UpdateAvailableTechniques(runtime);
}

static void OnDestroyEffectRuntime(reshade::api::effect_runtime* runtime) {
    if (!g_state) return;
    if (g_state->current_runtime == runtime) {
        g_state->current_runtime = nullptr;
    }
}

// Add-on init/cleanup
extern "C" __declspec(dllexport) bool AddonInit(HMODULE, HMODULE) {
    g_state = std::make_unique<AddonState>();
    AddLog("=== StreamerbotControl Initialized ===", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE, HMODULE) {
    if (!g_state) return;
    StopServer();
    g_state.reset();
}

// DLL entry point
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        if (!reshade::register_addon(hinstDLL)) return FALSE;
        reshade::register_event<reshade::addon_event::init_effect_runtime>(&OnInitEffectRuntime);
        reshade::register_event<reshade::addon_event::destroy_effect_runtime>(&OnDestroyEffectRuntime);
        reshade::register_overlay(ADDON_NAME, &OnDrawSettings);
        break;
    case DLL_PROCESS_DETACH:
        reshade::unregister_overlay(ADDON_NAME, &OnDrawSettings);
        reshade::unregister_event<reshade::addon_event::init_effect_runtime>(&OnInitEffectRuntime);
        reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(&OnDestroyEffectRuntime);
        reshade::unregister_addon(hinstDLL);
        break;
    }
    return TRUE;
}

// Metadata
extern "C" __declspec(dllexport) const char* NAME = "StreamerbotControl v1.1";
extern "C" __declspec(dllexport) const char* AUTHOR = "LonelyViper";
extern "C" __declspec(dllexport) const char* DESCRIPTION = ADDON_DESCRIPTION;
extern "C" __declspec(dllexport) const char* COPYRIGHT = ADDON_COPYRIGHT;