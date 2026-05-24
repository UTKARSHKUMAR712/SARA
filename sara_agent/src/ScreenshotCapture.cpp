#include "../include/ScreenshotCapture.h"
#include "../include/Logger.h"
#include <windows.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace sara {

ScreenshotResult ScreenshotCapture::capture_fullscreen(const std::string& output_dir) {
    ScreenshotResult result;
    namespace fs = std::filesystem;
    fs::create_directories(output_dir);

    HDC hdc_screen = GetDC(nullptr);
    HDC hdc_mem = CreateCompatibleDC(hdc_screen);

    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hbitmap = CreateCompatibleBitmap(hdc_screen, width, height);
    if (!hbitmap) {
        DeleteDC(hdc_mem);
        ReleaseDC(nullptr, hdc_screen);
        result.error = "CreateCompatibleBitmap failed";
        return result;
    }

    SelectObject(hdc_mem, hbitmap);
    BitBlt(hdc_mem, 0, 0, width, height, hdc_screen, 0, 0, SRCCOPY);

    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &tt);
    std::ostringstream oss;
    oss << output_dir << "\\screenshot_"
        << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".bmp";
    result.file_path = oss.str();

    BITMAPFILEHEADER bf = { 0 };
    BITMAPINFOHEADER bi = { 0 };
    BITMAP bmp;
    GetObject(hbitmap, sizeof(BITMAP), &bmp);

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;

    bf.bfType = 0x4D42;
    bf.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bi.biSizeImage;
    bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    HANDLE hfile = CreateFileA(result.file_path.c_str(), GENERIC_WRITE, 0,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hfile == INVALID_HANDLE_VALUE) {
        result.error = "Failed to create file";
        DeleteObject(hbitmap);
        DeleteDC(hdc_mem);
        ReleaseDC(nullptr, hdc_screen);
        return result;
    }

    DWORD written;
    WriteFile(hfile, &bf, sizeof(bf), &written, nullptr);
    WriteFile(hfile, &bi, sizeof(bi), &written, nullptr);

    std::vector<BYTE> pixels(bi.biSizeImage);
    GetDIBits(hdc_mem, hbitmap, 0, bmp.bmHeight, pixels.data(),
        (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    WriteFile(hfile, pixels.data(), pixels.size(), &written, nullptr);

    CloseHandle(hfile);
    DeleteObject(hbitmap);
    DeleteDC(hdc_mem);
    ReleaseDC(nullptr, hdc_screen);

    result.success = true;
    result.width = width;
    result.height = height;
    result.size_bytes = bf.bfSize;

    Logger::instance().info("Screenshot saved: " + result.file_path +
        " (" + std::to_string(width) + "x" + std::to_string(height) + ")");
    return result;
}

ScreenshotResult ScreenshotCapture::capture_window(const std::string& output_dir,
    const std::string& window_title)
{
    ScreenshotResult result;
    HWND hwnd = FindWindowA(nullptr, window_title.c_str());
    if (!hwnd) {
        result.error = "Window not found: " + window_title;
        return result;
    }

    RECT rect;
    GetWindowRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HDC hdc_window = GetDC(hwnd);
    HDC hdc_mem = CreateCompatibleDC(hdc_window);
    HBITMAP hbitmap = CreateCompatibleBitmap(hdc_window, width, height);
    SelectObject(hdc_mem, hbitmap);
    BitBlt(hdc_mem, 0, 0, width, height, hdc_window, 0, 0, SRCCOPY);

    namespace fs = std::filesystem;
    fs::create_directories(output_dir);

    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &tt);
    std::ostringstream oss;
    oss << output_dir << "\\screenshot_window_"
        << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".bmp";
    result.file_path = oss.str();

    BITMAPFILEHEADER bf = { 0 };
    BITMAPINFOHEADER bi = { 0 };
    BITMAP bmp;
    GetObject(hbitmap, sizeof(BITMAP), &bmp);

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;

    bf.bfType = 0x4D42;
    bf.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bi.biSizeImage;
    bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    HANDLE hfile = CreateFileA(result.file_path.c_str(), GENERIC_WRITE, 0,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hfile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hfile, &bf, sizeof(bf), &written, nullptr);
        WriteFile(hfile, &bi, sizeof(bi), &written, nullptr);
        std::vector<BYTE> pixels(bi.biSizeImage);
        GetDIBits(hdc_mem, hbitmap, 0, bmp.bmHeight, pixels.data(),
            (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        WriteFile(hfile, pixels.data(), pixels.size(), &written, nullptr);
        CloseHandle(hfile);
        result.success = true;
        result.size_bytes = bf.bfSize;
    }

    DeleteObject(hbitmap);
    DeleteDC(hdc_mem);
    ReleaseDC(hwnd, hdc_window);
    return result;
}

}
