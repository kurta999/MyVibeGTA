#include "renderer.h"
#include "../shaders/dx12_skin.h"

namespace game {
using namespace ::rendering::shaders;
bool Dx12Renderer::CreateSkinResources() {
    ID3DBlob* code = nullptr;
    if (!::rendering::CompileShader(win, skinShader, "CS", "cs_5_0", &code)) {
        Release(code);
        return false;
    }
    HRESULT result = m_device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(),
                                                   nullptr, &m_skinCS);
    Release(code);
    dx12::BufferDesc desc{};
    desc.ByteWidth = sizeof(SkinConstants);
    desc.Usage = dx12::Default;
    desc.BindFlags = dx12::Constant;
    if (SUCCEEDED(result))
        result = m_device->CreateBuffer(&desc, nullptr, &m_skinConstants);
    if (FAILED(result)) {
        Release(m_skinCS);
        Release(m_skinConstants);
        return false;
    }
    return true;
}

bool Dx12Renderer::CacheSkin(const dx11::SkinMesh* skin) {
    if (m_skinSources.count(skin))
        return true;
    if (skin->jointCount == 0 || skin->jointCount > dx11::MAX_GPU_SKIN_JOINTS ||
        skin->vertices.empty() || skin->vertices.size() > 300000)
        return false;
    static_assert(sizeof(dx11::SkinVertex) == 68 && sizeof(dx11::Vertex) == 48);
    struct ComputeVertex {
        dx11::SkinVertex skin;
        UINT parts = 0;
    };
    static_assert(sizeof(ComputeVertex) == 72);
    std::vector<ComputeVertex> source;
    source.reserve(skin->vertices.size());
    for (const auto& vertex : skin->vertices) {
        ComputeVertex v{vertex};
        for (unsigned i = 0; i < 4; ++i) {
            unsigned joint = vertex.joints[i];
            unsigned part =
                joint < skin->bodyPartForJoint.size() ? skin->bodyPartForJoint[joint] : 0;
            v.parts |= (part < 6 ? part : 0) << (i * 8);
        }
        source.push_back(v);
    }
    dx12::BufferDesc desc{};
    desc.ByteWidth = UINT(source.size() * sizeof(ComputeVertex));
    desc.Usage = dx12::Immutable;
    desc.BindFlags = dx12::ShaderInput;
    desc.MiscFlags = dx12::Structured;
    desc.StructureByteStride = sizeof(ComputeVertex);
    dx12::InitialData data{};
    data.pSysMem = source.data();
    dx12::Buffer* buffer = nullptr;
    dx12::ShaderResourceView* view = nullptr;
    HRESULT result = m_device->CreateBuffer(&desc, &data, &buffer);
    if (SUCCEEDED(result))
        result = m_device->CreateShaderResourceView(buffer, nullptr, &view);
    Release(buffer);
    if (FAILED(result)) {
        Release(view);
        return false;
    }
    m_skinSources.emplace(skin, view);
    return true;
}

bool Dx12Renderer::GrowSkinBuffer(size_t count) {
    if (count <= m_skinCapacity && m_skinOutput && m_skinOutputUav && m_skinPreviousOutput &&
        m_skinPreviousUav)
        return true;
    if (count > 3000000)
        return false;
    size_t capacity = std::min(size_t(3000000), std::max(count, m_skinCapacity * 2 + 100000));
    dx12::BufferDesc desc{};
    desc.ByteWidth = UINT(capacity * sizeof(dx11::Vertex));
    desc.Usage = dx12::Default;
    desc.BindFlags = dx12::Unordered | dx12::Vertex;
    desc.MiscFlags = dx12::Raw;
    dx12::Buffer* buffer = nullptr;
    dx12::UnorderedAccessView* view = nullptr;
    HRESULT result = m_device->CreateBuffer(&desc, nullptr, &buffer);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format = DXGI_FORMAT_R32_TYPELESS;
    uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav.Buffer.NumElements = desc.ByteWidth / 4;
    uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    if (SUCCEEDED(result))
        result = m_device->CreateUnorderedAccessView(buffer, &uav, &view);
    dx12::Buffer* previousBuffer = nullptr;
    dx12::UnorderedAccessView* previousView = nullptr;
    desc.ByteWidth = UINT(capacity * 16);
    uav.Buffer.NumElements = desc.ByteWidth / 4;
    if (SUCCEEDED(result))
        result = m_device->CreateBuffer(&desc, nullptr, &previousBuffer);
    if (SUCCEEDED(result))
        result = m_device->CreateUnorderedAccessView(previousBuffer, &uav, &previousView);
    if (FAILED(result)) {
        Release(buffer);
        Release(view);
        Release(previousBuffer);
        Release(previousView);
        return false;
    }
    Release(m_skinOutputUav);
    Release(m_skinOutput);
    Release(m_skinPreviousUav);
    Release(m_skinPreviousOutput);
    m_skinOutput = buffer;
    m_skinOutputUav = view;
    m_skinCapacity = capacity;
    m_skinPreviousOutput = previousBuffer;
    m_skinPreviousUav = previousView;
    return true;
}

bool Dx12Renderer::ValidateSkinOutput(const std::vector<dx11::SkinInstance>& previous,
                                      const std::vector<unsigned>& validity) {
    dx12::BufferDesc desc{};
    m_skinOutput->GetDesc(&desc);
    desc.Usage = dx12::Readback;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = dx12::Read;
    desc.MiscFlags = 0;
    dx12::Buffer* staging = nullptr;
    if (FAILED(m_device->CreateBuffer(&desc, nullptr, &staging)))
        return false;
    m_context->CopyResource(staging, m_skinOutput);
    dx12::MappedData mapped{};
    HRESULT result = m_context->Map(staging, 0, dx12::MapRead, 0, &mapped);
    bool valid = SUCCEEDED(result);
    float maxError = 0;
    if (valid) {
        std::vector<dx11::Vertex> expected;
        for (const auto& instance : m_gpuSkins)
            dx11::deformSkinCpu(instance, expected);
        const auto* actual = static_cast<const float*>(mapped.pData);
        for (size_t i = 0; i < expected.size() && valid; ++i) {
            float reference[12];
            std::memcpy(reference, &expected[i], sizeof(reference));
            for (int field = 0; field < 12; ++field) {
                float error = std::abs(actual[i * 12 + field] - reference[field]);
                maxError = std::max(maxError, error);
                float tolerance = field < 3 ? 0.003f : 0.00003f;
                if (!std::isfinite(actual[i * 12 + field]) || error > tolerance)
                    valid = false;
            }
        }
        m_context->Unmap(staging, 0);
    }
    Release(staging);
    m_skinPreviousOutput->GetDesc(&desc);
    desc.Usage = dx12::Readback;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = dx12::Read;
    desc.MiscFlags = 0;
    if (FAILED(m_device->CreateBuffer(&desc, nullptr, &staging)))
        return false;
    m_context->CopyResource(staging, m_skinPreviousOutput);
    result = m_context->Map(staging, 0, dx12::MapRead, 0, &mapped);
    if (SUCCEEDED(result)) {
        const auto* actual = static_cast<const float*>(mapped.pData);
        size_t start = 0;
        for (size_t pose = 0; pose < previous.size(); ++pose) {
            std::vector<dx11::Vertex> expected;
            dx11::deformSkinCpu(previous[pose], expected);
            for (size_t i = 0; i < expected.size(); ++i) {
                float reference[3] = {expected[i].x, expected[i].y, expected[i].z};
                for (int axis = 0; axis < 3; ++axis) {
                    float value = actual[(start + i) * 4 + axis];
                    float error = std::abs(value - reference[axis]);
                    maxError = std::max(maxError, error);
                    if (!std::isfinite(value) || error > 0.003f)
                        valid = false;
                }
                if (actual[(start + i) * 4 + 3] != float(validity[pose]))
                    valid = false;
            }
            start += expected.size();
        }
        m_context->Unmap(staging, 0);
    } else
        valid = false;
    Release(staging);
    char line[180]{};
    std::snprintf(line, sizeof(line),
                  "GPU skin validation: %s; frame %llu, %zu vertices, %zu poses, %u prior poses, "
                  "max error %.8f",
                  valid ? "PASS" : "FAIL", static_cast<unsigned long long>(m_skinFrame),
                  m_skinVertexCount, m_gpuSkins.size(),
                  unsigned(std::count(validity.begin(), validity.end(), 1u)), maxError);
    logging::write(line);
    return valid;
}

void Dx12Renderer::PrepareSkins() {
    ++m_skinFrame;
    m_skinVertexCount = 0;
    for (const auto& instance : m_gpuSkins)
        m_skinVertexCount += instance.source->vertices.size();
    if (m_skinVertexCount == 0)
        return;
    bool ready = m_skinCS && m_skinConstants && GrowSkinBuffer(m_skinVertexCount);
    for (const auto& instance : m_gpuSkins)
        ready = ready && instance.palette.size() == instance.source->jointCount &&
                CacheSkin(instance.source);
    if (ready) {
        const bool validate = std::strstr(GetCommandLineA(), "--validate-gpu-skinning") &&
                              (m_skinFrame == 1 || m_skinFrame == 30 || m_skinFrame == 60);
        std::vector<dx11::SkinInstance> priorPoses;
        std::vector<unsigned> priorValidity;
        // A previous frame may still have this output bound to the input assembler.
        dx12::Buffer* emptyBuffers[2]{};
        UINT zeros[2]{};
        m_context->IASetVertexBuffers(0, 2, emptyBuffers, zeros, zeros);
        m_context->CSSetShader(m_skinCS, nullptr, 0);
        m_context->CSSetConstantBuffers(0, 1, &m_skinConstants);
        dx12::UnorderedAccessView* outputViews[2] = {m_skinOutputUav, m_skinPreviousUav};
        m_context->CSSetUnorderedAccessViews(0, 2, outputViews, nullptr);
        UINT start = 0;
        for (const auto& instance : m_gpuSkins) {
            SkinConstants constants{};
            std::copy(instance.palette.begin(), instance.palette.end(), constants.palette);
            constants.scale = instance.scale;
            constants.origin = instance.origin;
            constants.transform = instance.transform;
            constants.yaw = instance.yaw;
            constants.deformation = instance.deformation;
            constants.count = UINT(instance.source->vertices.size());
            constants.start = start;
            constants.joints = instance.source->jointCount;
            auto prior = m_previousSkins.find(instance.identity);
            bool valid = instance.identity != 0 && prior != m_previousSkins.end() &&
                         prior->second.m_frame + 1 == m_skinFrame &&
                         prior->second.m_pose.source == instance.source;
            if (valid) {
                float distanceSquared = 0;
                for (int axis = 0; axis < 3; ++axis) {
                    float delta = instance.transform[axis] - prior->second.m_pose.transform[axis];
                    distanceSquared += delta * delta;
                }
                valid = distanceSquared < 65 * 65;
            }
            const auto& previous = valid ? prior->second.m_pose : instance;
            std::copy(previous.palette.begin(), previous.palette.end(), constants.previousPalette);
            constants.previousScale = previous.scale;
            constants.previousOrigin = previous.origin;
            constants.previousTransform = previous.transform;
            constants.previousYaw = previous.yaw;
            constants.previousDeformation = previous.deformation;
            constants.padding = valid ? 1 : 0;
            if (validate) {
                priorPoses.push_back(previous);
                priorValidity.push_back(constants.padding);
            }
            m_context->UpdateSubresource(m_skinConstants, 0, nullptr, &constants, 0, 0);
            auto* source = m_skinSources.at(instance.source);
            m_context->CSSetShaderResources(0, 1, &source);
            m_context->Dispatch((constants.count + 63) / 64, 1, 1);
            start += constants.count;
        }
        dx12::UnorderedAccessView* emptyUavs[2]{};
        dx12::ShaderResourceView* emptyView = nullptr;
        m_context->CSSetUnorderedAccessViews(0, 2, emptyUavs, nullptr);
        m_context->CSSetShaderResources(0, 1, &emptyView);
        m_context->CSSetShader(nullptr, nullptr, 0);
        if (!m_skinValidationDone) {
            char line[140]{};
            std::snprintf(line, sizeof(line),
                          "GPU skinning: %zu poses including procedural/aim/rigid attachments, %zu "
                          "vertices; shared color/shadow buffer",
                          m_gpuSkins.size(), m_skinVertexCount);
            logging::write(line);
            m_skinValidationDone = true;
        }
        if (validate)
            ready = ValidateSkinOutput(priorPoses, priorValidity);
        for (auto item = m_previousSkins.begin(); item != m_previousSkins.end();) {
            if (item->second.m_frame + 1 < m_skinFrame)
                item = m_previousSkins.erase(item);
            else
                ++item;
        }
        for (const auto& instance : m_gpuSkins)
            if (instance.identity != 0)
                m_previousSkins.insert_or_assign(instance.identity,
                                                 PreviousSkin{instance, m_skinFrame});
    }
    if (!ready) {
        for (const auto& instance : m_gpuSkins)
            dx11::deformSkinCpu(instance, m_groups[5]);
        m_skinVertexCount = 0;
        Release(m_skinCS);
        logging::write("GPU skinning unavailable; using CPU deformation");
    }
}

void Dx12Renderer::DrawSkins(bool shadow, const SceneConstants& frame) {
    if (!m_skinVertexCount)
        return;
    SceneConstants constants = frame;
    constants.m_params.x = 5;
    constants.m_temporalInfo.x = 0;
    constants.m_temporalInfo.y = shadow ? 0.0f : 1.0f;
    constants.m_materialPbr = PbrForGroup(5);
    constants.m_materialSurface = {0, 0.1f, 0, 0};
    constants.m_materialOptions = {0, 0, 0, 0};
    m_context->UpdateSubresource(m_sceneBuffer, 0, nullptr, &constants, 0, 0);
    m_context->IASetInputLayout(m_skinLayout);
    UINT strides[2] = {sizeof(dx11::Vertex), 16}, offsets[2]{};
    dx12::Buffer* buffers[2] = {m_skinOutput, m_skinPreviousOutput};
    m_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
    m_context->VSSetShader(m_skinVS, nullptr, 0);
    m_context->PSSetShader(shadow ? nullptr : m_scenePS, nullptr, 0);
    SetTessellation(5);
    if (!shadow) {
        dx12::ShaderResourceView* resources[4] = {nullptr, m_detailTextures[5], m_normalTextures[5],
                                                  m_detailTextures[9]};
        m_context->PSSetShaderResources(0, 4, resources);
    }
    m_context->Draw(UINT(m_skinVertexCount), 0);
    ++drawCalls;
    triangleCount += m_skinVertexCount / 3;
}
} // namespace game
