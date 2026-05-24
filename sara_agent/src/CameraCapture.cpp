#include "../include/CameraCapture.h"
#include "../include/Logger.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <algorithm>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using namespace Microsoft::WRL;

namespace sara {

CameraResult try_mf(const std::string& path, int device_index) {
    CameraResult r;

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) { r.error = "MFStartup failed"; return r; }

    ComPtr<IMFAttributes> attr;
    hr = MFCreateAttributes(&attr, 1);
    if (FAILED(hr)) { MFShutdown(); r.error = "MFCreateAttributes failed"; return r; }

    hr = attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) { MFShutdown(); r.error = "SetGUID failed"; return r; }

    IMFActivate** activate_list = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(attr.Get(), &activate_list, &count);
    if (FAILED(hr) || count == 0 || (UINT32)device_index >= count) {
        if (count == 0) r.error = "No camera found";
        else r.error = "Camera index " + std::to_string(device_index) + " out of range";
        if (activate_list) {
            for (UINT32 i = 0; i < count; i++) activate_list[i]->Release();
            CoTaskMemFree(activate_list);
        }
        MFShutdown(); return r;
    }

    ComPtr<IMFMediaSource> source;
    hr = activate_list[device_index]->ActivateObject(IID_PPV_ARGS(&source));
    for (UINT32 i = 0; i < count; i++) activate_list[i]->Release();
    CoTaskMemFree(activate_list);
    if (FAILED(hr)) { MFShutdown(); r.error = "ActivateObject failed"; return r; }

    ComPtr<IMFSourceReader> reader;
    hr = MFCreateSourceReaderFromMediaSource(source.Get(), nullptr, &reader);
    if (FAILED(hr)) { source->Shutdown(); MFShutdown(); r.error = "CreateSourceReader failed"; return r; }

    ComPtr<IMFMediaType> media_type;
    hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &media_type);
    if (FAILED(hr)) { source->Shutdown(); MFShutdown(); r.error = "GetCurrentMediaType failed"; return r; }

    UINT32 w = 0, h = 0;
    MFGetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, &w, &h);
    if (w == 0 || h == 0) { w = 640; h = 480; }

    GUID subtype;
    media_type->GetGUID(MF_MT_SUBTYPE, &subtype);

    int y_stride = (w * 4 + 15) & ~15;
    {
        DWORD fmt = 0;
        if (subtype == MFVideoFormat_NV12) fmt = MAKEFOURCC('N','V','1','2');
        else if (subtype == MFVideoFormat_YUY2) fmt = MAKEFOURCC('Y','U','Y','2');
        else if (subtype == MFVideoFormat_I420 || subtype == MFVideoFormat_IYUV) fmt = MAKEFOURCC('I','4','2','0');
        if (fmt) MFGetStrideForBitmapInfoHeader(fmt, w, (LONG*)&y_stride);
        if (y_stride < 0) y_stride = -y_stride;
        if (y_stride < (int)w) y_stride = w + (15 - (w % 16));
    }

    Logger::instance().info("Camera: " + std::to_string(w) + "x" + std::to_string(h) +
                            " stride=" + std::to_string(y_stride));

    ComPtr<IMFSample> sample;
    DWORD stream_flags = 0;
    for (int retry = 0; retry < 60; retry++) {
        hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
            nullptr, &stream_flags, nullptr, &sample);
        if (SUCCEEDED(hr) && sample) break;
        if (stream_flags & MF_SOURCE_READERF_STREAMTICK) sample.Reset();
        Sleep(100);
    }

    if (!sample) {
        source->Shutdown(); MFShutdown();
        r.error = "No frame captured from camera";
        return r;
    }

    ComPtr<IMFMediaBuffer> buf;
    hr = sample->ConvertToContiguousBuffer(&buf);
    if (FAILED(hr)) { source->Shutdown(); MFShutdown(); r.error = "Get buffer failed"; return r; }

    BYTE* data = nullptr;
    DWORD data_len = 0;
    hr = buf->Lock(&data, nullptr, &data_len);
    if (FAILED(hr)) { source->Shutdown(); MFShutdown(); r.error = "Lock buffer failed"; return r; }

    // Detect NV12: Y plane = stride * h, chroma = stride * (h/2)
    int y_plane_bytes = y_stride * h;
    bool is_packed_yuv = false;
    std::vector<uint8_t> rgb_buf(w * h * 4);

    if (subtype == MFVideoFormat_YUY2 || data_len == (DWORD)(w * h * 2)) {
        is_packed_yuv = true;
        for (int y = 0; y < h; y++) {
            const uint8_t* row = data + y * y_stride;
            uint8_t* dst = rgb_buf.data() + y * w * 4;
            for (int x = 0; x < w; x++) {
                int Y0 = row[(x / 2) * 4];
                int U  = row[(x / 2) * 4 + 1] - 128;
                int Y1 = row[(x / 2) * 4 + 2];
                int V  = row[(x / 2) * 4 + 3] - 128;
                int Y = (x % 2 == 0) ? Y0 : Y1;
                int C = Y - 16;
                int R = (298 * C + 409 * V + 128) >> 8;
                int G = (298 * C - 100 * U - 208 * V + 128) >> 8;
                int B = (298 * C + 516 * U + 128) >> 8;
                dst[x * 4]     = (uint8_t)std::clamp(B, 0, 255);
                dst[x * 4 + 1] = (uint8_t)std::clamp(G, 0, 255);
                dst[x * 4 + 2] = (uint8_t)std::clamp(R, 0, 255);
                dst[x * 4 + 3] = 255;
            }
        }
    } else if (subtype == MFVideoFormat_NV12 ||
               data_len == (DWORD)(y_plane_bytes + (y_stride * h / 2))) {
        const uint8_t* y_plane = data;
        const uint8_t* uv_plane = data + y_plane_bytes;
        for (int y = 0; y < h; y++) {
            const uint8_t* y_row = y_plane + y * y_stride;
            const uint8_t* uv_row = uv_plane + (y / 2) * y_stride;
            uint8_t* dst = rgb_buf.data() + y * w * 4;
            for (int x = 0; x < w; x++) {
                int Y = y_row[x];
                int U = uv_row[(x / 2) * 2] - 128;
                int V = uv_row[(x / 2) * 2 + 1] - 128;
                int C = Y - 16;
                int R = (298 * C + 409 * V + 128) >> 8;
                int G = (298 * C - 100 * U - 208 * V + 128) >> 8;
                int B = (298 * C + 516 * U + 128) >> 8;
                dst[x * 4]     = (uint8_t)std::clamp(B, 0, 255);
                dst[x * 4 + 1] = (uint8_t)std::clamp(G, 0, 255);
                dst[x * 4 + 2] = (uint8_t)std::clamp(R, 0, 255);
                dst[x * 4 + 3] = 255;
            }
        }
    } else {
        // Unknown format, treat as packed 24-bit RGB
        int src_stride = (w * 3 + 3) & ~3;
        for (int y = 0; y < h && (DWORD)(y * src_stride) < data_len; y++) {
            const uint8_t* src_row = data + y * y_stride;
            uint8_t* dst = rgb_buf.data() + y * w * 4;
            for (int x = 0; x < w; x++) {
                dst[x * 4]     = src_row[x * 3];
                dst[x * 4 + 1] = src_row[x * 3 + 1];
                dst[x * 4 + 2] = src_row[x * 3 + 2];
                dst[x * 4 + 3] = 255;
            }
        }
    }

    buf->Unlock();
    source->Shutdown();
    MFShutdown();

    r = CameraCapture::save_bmp(path, rgb_buf.data(), w, h);
    return r;
}

CameraResult CameraCapture::save_bmp(const std::string& path, const uint8_t* rgb_data, int w, int h) {
    CameraResult r;
    r.file_path = path;
    int pitch = w * 4;

    BITMAPFILEHEADER bf;
    BITMAPINFOHEADER bi;
    ZeroMemory(&bf, sizeof(bf));
    ZeroMemory(&bi, sizeof(bi));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = pitch * h;

    bf.bfType = 0x4D42;
    bf.bfSize = sizeof(bf) + sizeof(bi) + bi.biSizeImage;
    bf.bfOffBits = sizeof(bf) + sizeof(bi);

    HANDLE hfile = CreateFileA(path.c_str(), GENERIC_WRITE, 0,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hfile == INVALID_HANDLE_VALUE) {
        r.error = "Failed to create file";
        return r;
    }

    DWORD written;
    WriteFile(hfile, &bf, sizeof(bf), &written, nullptr);
    WriteFile(hfile, &bi, sizeof(bi), &written, nullptr);
    for (int y = h - 1; y >= 0; y--) {
        WriteFile(hfile, rgb_data + y * pitch, pitch, &written, nullptr);
    }
    CloseHandle(hfile);

    r.success = true;
    Logger::instance().info("Camera capture saved: " + r.file_path + " (" +
                            std::to_string(w) + "x" + std::to_string(h) + ")");
    return r;
}

CameraResult CameraCapture::capture(const std::string& output_dir, int device_index) {
    CameraResult result;
    namespace fs = std::filesystem;
    fs::create_directories(output_dir);

    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &tt);
    std::ostringstream oss;
    oss << output_dir << "\\camera_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".bmp";
    result.file_path = oss.str();

    result = try_mf(result.file_path, device_index);
    return result;
}

}
