#include "remote_runtime/include/LiveServer.h"
#include <iostream>
#include <windows.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    sara::remote::LiveServer server;
    if (server.start("frontend\\lan_dashboard", 3000, true)) {
        std::cout << "Started on 3000" << std::endl;
        Sleep(60000); // run for 60s
    } else {
        std::cout << "Failed to start" << std::endl;
    }
    return 0;
}
