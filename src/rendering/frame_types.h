#pragma once
#include "../dx11_assets.h"
#include <DirectXMath.h>
#include <cstdint>

namespace rendering {
using DirectX::XMFLOAT4;
using DirectX::XMFLOAT4X4;
struct ProbeConstants {
    XMFLOAT4 m_positions[2], m_sh[3][9], m_timeWeights, m_info;
};
struct PreviousSkin {
    dx11::SkinInstance m_pose;
    std::uint64_t m_frame;
};
struct InstanceData {
    XMFLOAT4 m_a, m_b, m_c, m_tint, m_quaternion;
};
struct InstanceBatch {
    const dx11::Mesh* m_mesh;
    int m_material;
    std::uint32_t m_start, m_count;
};
struct SceneConstants {
    XMFLOAT4X4 m_viewProjection;
    XMFLOAT4X4 m_shadowViewProjection[3];
    XMFLOAT4 m_sun;
    XMFLOAT4 m_ambient;
    XMFLOAT4 m_fogColor;
    XMFLOAT4 m_eye;
    XMFLOAT4 m_params;
    XMFLOAT4 m_weatherAndTime;
    XMFLOAT4 m_materialPbr;
    XMFLOAT4 m_materialSurface;
    XMFLOAT4 m_materialOptions;
    XMFLOAT4 m_shadowInfo;
    XMFLOAT4 m_localLightPosition[12];
    XMFLOAT4 m_localLightColor[12];
    XMFLOAT4X4 m_headlightViewProjection;
    XMFLOAT4 m_headlightShadowInfo;
    XMFLOAT4X4 m_streetViewProjection;
    XMFLOAT4 m_streetShadowInfo;
    XMFLOAT4 m_temporalInfo;
    XMFLOAT4X4 m_previousViewProjection;
};
} // namespace rendering
