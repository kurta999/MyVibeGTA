#include "screenshot_writer.h"
#include "loading.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cwchar>
#include <vector>

namespace rendering {
bool ScreenshotWriter::SavePng(void* pixels, unsigned rowPitch, unsigned width, unsigned height,
                               std::string& filename) {
    std::wstring folder = ExecutableFolder() + L"\\screenshots";
    if (!CreateDirectoryW(folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    UINT encoderCount = 0, encoderBytes = 0;
    if (Gdiplus::GetImageEncodersSize(&encoderCount, &encoderBytes) != Gdiplus::Ok ||
        encoderCount == 0 || encoderBytes == 0)
        return false;
    std::vector<unsigned char> storage(encoderBytes);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
    if (Gdiplus::GetImageEncoders(encoderCount, encoderBytes, encoders) != Gdiplus::Ok)
        return false;
    const CLSID* pngEncoder = nullptr;
    for (UINT i = 0; i < encoderCount; ++i)
        if (encoders[i].MimeType && std::wcscmp(encoders[i].MimeType, L"image/png") == 0) {
            pngEncoder = &encoders[i].Clsid;
            break;
        }
    if (!pngEncoder)
        return false;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t name[100]{};
    for (int attempt = 0; attempt < 1000; ++attempt) {
        std::swprintf(name, 100, L"MiniCity3D-%04u%02u%02u-%02u%02u%02u-%03u-%lu-%u.png", now.wYear,
                      now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
                      GetCurrentProcessId(), m_sequence++);
        std::wstring path = folder + L"\\" + name;
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            continue;
        Gdiplus::Bitmap bitmap(width, height, INT(rowPitch), PixelFormat32bppARGB,
                               static_cast<BYTE*>(pixels));
        if (bitmap.GetLastStatus() != Gdiplus::Ok ||
            bitmap.Save(path.c_str(), pngEncoder, nullptr) != Gdiplus::Ok)
            return false;
        filename.clear();
        for (const wchar_t* character = name; *character; ++character)
            filename.push_back(static_cast<char>(*character));
        return true;
    }
    return false;
}
} // namespace rendering
