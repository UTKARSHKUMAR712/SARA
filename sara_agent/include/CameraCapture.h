#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

struct CameraResult {
    bool success = false;
    std::string file_path;
    std::string error;
};

class CameraCapture {
public:
    CameraResult capture(const std::string& output_dir, int device_index = 0);
    static CameraResult save_bmp(const std::string& path, const uint8_t* rgb_data, int w, int h);
    static CameraResult save_bmp_raw(const std::string& path, const uint8_t* data, int w, int h, unsigned long data_len);
};

}
