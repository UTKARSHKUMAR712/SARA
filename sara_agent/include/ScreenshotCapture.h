#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

struct ScreenshotResult {
    bool success = false;
    std::string file_path;
    std::string error;
    int width = 0;
    int height = 0;
    size_t size_bytes = 0;
};

class ScreenshotCapture {
public:
    ScreenshotResult capture_fullscreen(const std::string& output_dir);
    ScreenshotResult capture_window(const std::string& output_dir,
        const std::string& window_title);
};

}
