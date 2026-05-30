#pragma once
#include <string>
#include <cstdint>

namespace sara::remote {

// Generate a 64-char hex token using BCryptGenRandom (cryptographically secure)
std::string generate_token();

// Generate an 8-char alphanumeric session ID
std::string generate_session_id();

// Get current Unix timestamp in seconds
int64_t now_seconds();

} // namespace sara::remote
