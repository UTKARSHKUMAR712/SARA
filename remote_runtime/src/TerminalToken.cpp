#include "../include/TerminalToken.h"
#include <windows.h>
#include <bcrypt.h>
#include <chrono>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "bcrypt.lib")

namespace sara::remote {

std::string generate_token() {
    BYTE bytes[32] = {};
    BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : bytes) oss << std::setw(2) << (int)b;
    return oss.str(); // 64-char hex
}

std::string generate_session_id() {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    BYTE bytes[8] = {};
    BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    std::string id;
    id.reserve(8);
    for (auto b : bytes) id += charset[b % (sizeof(charset) - 1)];
    return id;
}

int64_t now_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace sara::remote
