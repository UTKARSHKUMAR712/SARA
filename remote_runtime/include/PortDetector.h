#pragma once
#include <vector>
#include <string>

namespace sara::remote {

struct ActivePort {
    int port;
    std::string status;
};

class PortDetector {
public:
    // Returns a list of active ports bound to 127.0.0.1 or 0.0.0.0
    // Filters out system and high ephemeral ports.
    static std::vector<ActivePort> get_active_ports();
};

} // namespace sara::remote
