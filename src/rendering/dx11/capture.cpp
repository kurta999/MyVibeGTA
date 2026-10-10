#include "renderer.h"

namespace game {

void Dx11Renderer::CaptureIfRequested() {
    char file[MAX_PATH]{};
    DWORD legacyLength = GetEnvironmentVariableA("MINICITY_CAPTURE", file, MAX_PATH);
    bool legacyCapture = legacyLength > 0 && legacyLength < MAX_PATH;
    bool pngCapture = screenshotRequested;
    if (!legacyCapture && !pngCapture)
        return;
    screenshotRequested = false;
    if (legacyCapture)
        SetEnvironmentVariableA("MINICITY_CAPTURE", nullptr);
    ID3D11Texture2D* back = nullptr;
    if (FAILED(
            m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back))))
        return;
    D3D11_TEXTURE2D_DESC description{};
    back->GetDesc(&description);
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    if (SUCCEEDED(m_device->CreateTexture2D(&description, nullptr, &staging))) {
        m_context->CopyResource(staging, back);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            if (pngCapture) {
                std::string name;
                if (m_screenshotWriter.SavePng(mapped.pData, mapped.RowPitch, description.Width,
                                               description.Height, name)) {
                    message = "Screenshot saved: screenshots/" + name;
                    logging::write(message.c_str());
                } else {
                    message = "Screenshot failed to save";
                    logging::write(message.c_str());
                }
                messageTime = 3.0f;
            }
            if (legacyCapture) {
                BITMAPFILEHEADER fileHeader{};
                BITMAPINFOHEADER imageHeader{};
                fileHeader.bfType = 0x4D42;
                fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(imageHeader);
                fileHeader.bfSize =
                    fileHeader.bfOffBits + description.Width * description.Height * 4;
                imageHeader.biSize = sizeof(imageHeader);
                imageHeader.biWidth = LONG(description.Width);
                imageHeader.biHeight = -LONG(description.Height);
                imageHeader.biPlanes = 1;
                imageHeader.biBitCount = 32;
                imageHeader.biCompression = BI_RGB;
                std::ofstream output(file, std::ios::binary);
                if (output) {
                    output.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
                    output.write(reinterpret_cast<const char*>(&imageHeader), sizeof(imageHeader));
                    for (UINT row = 0; row < description.Height; ++row)
                        output.write(reinterpret_cast<const char*>(
                                         static_cast<const unsigned char*>(mapped.pData) +
                                         size_t(row) * mapped.RowPitch),
                                     size_t(description.Width) * 4);
                }
            }
            m_context->Unmap(staging, 0);
        }
    }
    Release(staging);
    Release(back);
}
} // namespace game
