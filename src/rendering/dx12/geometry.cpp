#include "renderer.h"

namespace game {

bool Dx12Renderer::CacheModel(const dx11::Mesh* source) {
    return source && m_meshCache->Ensure(*source) && m_textureLibrary->EnsureMesh(*source);
}

float Dx12Renderer::ShadowCascadeExtent(int cascade) {
    const float scale = ui::drawDistanceScale();
    const float nearDistances[3] = {2.0f, 250.0f * scale, 650.0f * scale};
    const float farDistances[3] = {250.0f * scale, 650.0f * scale, 1250.0f * scale};
    const float halfDepth = (farDistances[cascade] - nearDistances[cascade]) * 0.5f;
    const float halfHeight =
        farDistances[cascade] * std::tan(XMConvertToRadians(camera::fieldOfView()) * 0.5f);
    const float halfWidth = halfHeight * float(m_bufferW) / float(m_bufferH);
    // Fit the complete camera-frustum segment in a light-space sphere so a
    // turn cannot expose unshadowed corners at a cascade boundary.
    return 2.12f *
           std::sqrt(halfDepth * halfDepth + halfHeight * halfHeight + halfWidth * halfWidth);
}

bool Dx12Renderer::PrepareInstances(const camera::Pose& pose, const SceneConstants& constants) {
    m_instanceData.clear();
    m_instanceBatches.clear();
    for (auto& batches : m_shadowInstanceBatches)
        batches.clear();
    m_headlightShadowInstanceBatches.clear();
    m_streetShadowInstanceBatches.clear();
    const bool disableCulling = std::strstr(GetCommandLineA(), "--no-frustum-cull") != nullptr;
    const bool receiverCull =
        !disableCulling && !std::strstr(GetCommandLineA(), "--legacy-shadow-culling");
    const bool shadowLod = !std::strstr(GetCommandLineA(), "--legacy-shadow-lod");
    const dx12::ShadowReceivers receivers(
        constants.m_viewProjection, {pose.eye.x, pose.eye.y, pose.eye.z},
        {constants.m_sun.x, constants.m_sun.y, constants.m_sun.z});
    std::vector<dx11::BoundingSphere> bounds;
    bounds.reserve(m_models.size());
    for (const auto& model : m_models)
        bounds.push_back(dx11::instanceBounds(model));
    auto modelBounds = [&](const dx11::ModelInstance& model) -> const dx11::BoundingSphere& {
        return bounds[&model - m_models.data()];
    };
    std::unordered_set<const dx11::Mesh*> cachedThisFrame;
    auto ensureCached = [&](const dx11::Mesh* source) {
        return !cachedThisFrame.insert(source).second || CacheModel(source);
    };
    XMVECTOR eye = XMVectorSet(pose.eye.x, pose.eye.y, pose.eye.z, 1);
    XMVECTOR forward = XMVector3Normalize(XMVectorSet(
        pose.target.x - pose.eye.x, pose.target.y - pose.eye.y, pose.target.z - pose.eye.z, 0));
    XMVECTOR cameraUp = XMVectorSet(0, 1, 0, 0);
    if (m_probeBakeActive && m_probeBakeFrame % 6 == 2)
        cameraUp = XMVectorSet(0, 0, -1, 0);
    if (m_probeBakeActive && m_probeBakeFrame % 6 == 3)
        cameraUp = XMVectorSet(0, 0, 1, 0);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, cameraUp));
    XMVECTOR up = XMVector3Cross(right, forward);
    const float tangent =
        m_probeBakeActive ? 1.0f : std::tan(XMConvertToRadians(camera::fieldOfView()) * 0.5f);
    const float aspect = float(m_bufferW) / m_bufferH;
    const float farPlane = std::max(5000.0f, 1250.0f * ui::drawDistanceScale());
    XMMATRIX shadowMatrices[3]{XMLoadFloat4x4(&constants.m_shadowViewProjection[0]),
                               XMLoadFloat4x4(&constants.m_shadowViewProjection[1]),
                               XMLoadFloat4x4(&constants.m_shadowViewProjection[2])};
    XMMATRIX headlightMatrix = XMLoadFloat4x4(&constants.m_headlightViewProjection);
    XMMATRIX streetMatrix = XMLoadFloat4x4(&constants.m_streetViewProjection);
    auto visibleInCamera = [&](const dx11::BoundingSphere& sphere) {
        XMVECTOR delta = XMVectorSubtract(XMVectorSet(sphere.x, sphere.y, sphere.z, 1), eye);
        float depth = XMVectorGetX(XMVector3Dot(delta, forward));
        float radius = sphere.radius * 1.5f;
        if (depth < -radius || depth > farPlane + radius)
            return false;
        float halfHeight = std::max(0.0f, depth) * tangent;
        return std::abs(XMVectorGetX(XMVector3Dot(delta, right))) <= halfHeight * aspect + radius &&
               std::abs(XMVectorGetX(XMVector3Dot(delta, up))) <= halfHeight + radius;
    };
    auto visibleInShadow = [&](const dx11::BoundingSphere& sphere, int cascade) {
        XMVECTOR projected = XMVector3TransformCoord(XMVectorSet(sphere.x, sphere.y, sphere.z, 1),
                                                     shadowMatrices[cascade]);
        float extent = m_shadowCascadeCount > 1 ? ShadowCascadeExtent(cascade)
                                                : 1800.0f * ui::drawDistanceScale();
        float xyRadius = sphere.radius * 2.0f / extent;
        float zRadius = sphere.radius / 3300.0f;
        return std::abs(XMVectorGetX(projected)) <= 1 + xyRadius &&
               std::abs(XMVectorGetY(projected)) <= 1 + xyRadius &&
               XMVectorGetZ(projected) >= -zRadius && XMVectorGetZ(projected) <= 1 + zRadius;
    };
    auto visibleInLocal = [&](const dx11::BoundingSphere& sphere, XMMATRIX matrix, float farPlane) {
        XMVECTOR clip = XMVector4Transform(XMVectorSet(sphere.x, sphere.y, sphere.z, 1), matrix);
        float w = XMVectorGetW(clip), radius = sphere.radius * 2.0f;
        return w >= -radius && w <= farPlane + radius &&
               std::abs(XMVectorGetX(clip)) <= w + radius &&
               std::abs(XMVectorGetY(clip)) <= w + radius && XMVectorGetZ(clip) >= -radius &&
               XMVectorGetZ(clip) <= w + radius;
    };
    auto append = [&](const dx11::ModelInstance& model, std::vector<InstanceBatch>& batches,
                      bool separate, const dx11::Mesh* selected = nullptr) {
        if (!selected)
            selected = model.source;
        if (batches.empty() || separate || batches.back().m_mesh != selected ||
            batches.back().m_material != model.material)
            batches.push_back({selected, model.material, UINT(m_instanceData.size()), 0});
        ++batches.back().m_count;
        m_instanceData.push_back({{model.scaleX, model.scaleY, model.scaleZ, model.cosYaw},
                                  {model.sinYaw, model.x, model.y, model.z},
                                  {model.centerX, model.minY, model.centerZ, model.sinPitch},
                                  {model.r, model.g, model.b, model.cosPitch},
                                  {model.qx, model.qy, model.qz, model.qw}});
    };
    for (const auto& model : m_models) {
        if (!disableCulling && !visibleInCamera(modelBounds(model)))
            continue;
        if (!ensureCached(model.source))
            return false;
        append(model, m_instanceBatches, model.source->transparent);
    }
    if (constants.m_params.w > 0) {
        std::vector<std::pair<const dx11::ModelInstance*, const dx11::Mesh*>> casters;
        casters.reserve(m_models.size());
        for (int cascade = 0; cascade < m_shadowCascadeCount; ++cascade) {
            casters.clear();
            const float extent = m_shadowCascadeCount > 1 ? ShadowCascadeExtent(cascade)
                                                          : 1800.0f * ui::drawDistanceScale();
            // The shader chooses cascades by radial distance and blends across
            // each split. The last cascade has no radial cutoff in the shader.
            const float receiverRange = m_shadowCascadeCount < 2 || cascade == 2
                                            ? std::numeric_limits<float>::infinity()
                                        : cascade == 0 ? constants.m_shadowInfo.y * 1.08f
                                                       : constants.m_shadowInfo.z * 1.06f;
            const float padding = 4 * extent / std::max(1, m_shadowSize);
            for (const auto& model : m_models) {
                if (!model.source->castsShadow ||
                    (!disableCulling && !visibleInShadow(modelBounds(model), cascade)))
                    continue;
                ++m_geometryStats.m_casterCandidates;
                const auto& sphere = modelBounds(model);
                if (receiverCull && !receivers.reaches(sphere, receiverRange, padding)) {
                    ++m_geometryStats.m_receiverCulled;
                    continue;
                }
                const auto* selected =
                    shadowLod                   ? dx11::shadowLodMesh(model.source,
                                                                      2 * sphere.radius * m_shadowSize / extent)
                    : model.source->shadowProxy ? model.source->shadowProxy
                                                : model.source;
                if (selected != model.source && selected != model.source->shadowProxy)
                    ++m_geometryStats.m_lodInstances;
                if (!ensureCached(selected))
                    return false;
                casters.emplace_back(&model, selected);
            }
            // Camera LODs can collapse to the same shadow mesh. Regroup after
            // selection so those instances share one draw even when their
            // camera batches were separated by other meshes.
            std::stable_sort(casters.begin(), casters.end(), [](const auto& a, const auto& b) {
                if (a.first->material != b.first->material)
                    return a.first->material < b.first->material;
                return std::less<const dx11::Mesh*>{}(a.second, b.second);
            });
            for (const auto& caster : casters)
                append(*caster.first, m_shadowInstanceBatches[cascade], false, caster.second);
        }
    }
    if (constants.m_headlightShadowInfo.z > 0.5f) {
        for (const auto& model : m_models) {
            if (!model.source->castsShadow ||
                (!disableCulling && !visibleInLocal(modelBounds(model), headlightMatrix, 145.0f)))
                continue;
            if (!ensureCached(model.source))
                return false;
            if (model.source->shadowProxy && !ensureCached(model.source->shadowProxy))
                return false;
            append(model, m_headlightShadowInstanceBatches, false);
        }
    }
    if (constants.m_streetShadowInfo.y > 0.5f) {
        for (const auto& model : m_models) {
            if (!model.source->castsShadow ||
                (!disableCulling && !visibleInLocal(modelBounds(model), streetMatrix, 130.0f)))
                continue;
            if (!ensureCached(model.source))
                return false;
            if (model.source->shadowProxy && !ensureCached(model.source->shadowProxy))
                return false;
            append(model, m_streetShadowInstanceBatches, false);
        }
    }
    if (m_instanceData.empty())
        return true;
    if (!m_dynamicInstances->Ensure(m_instanceData.size()))
        return false;
    auto* instanceBuffer = m_dynamicInstances->Get();
    dx12::MappedData mapped{};
    if (FAILED(m_context->Map(instanceBuffer, 0, dx12::MapWrite, 0, &mapped)))
        return false;
    std::memcpy(mapped.pData, m_instanceData.data(), m_instanceData.size() * sizeof(InstanceData));
    m_context->Unmap(instanceBuffer, 0);
    return true;
}

bool Dx12Renderer::CreateStaticGeometry() {
    std::vector<dx11::Vertex> staticGroups[dx11::MATERIAL_GROUPS];
    dx11::buildStaticScene(staticGroups);
    std::vector<dx11::Vertex> packed;
    for (int group = 0; group < dx11::MATERIAL_GROUPS; ++group) {
        m_staticStarts[group] = packed.size();
        m_staticCounts[group] = staticGroups[group].size();
        packed.insert(packed.end(), staticGroups[group].begin(), staticGroups[group].end());
    }
    if (packed.empty())
        return false;
    dx12::BufferDesc desc{};
    desc.ByteWidth = UINT(packed.size() * sizeof(dx11::Vertex));
    desc.Usage = dx12::Immutable;
    desc.BindFlags = dx12::Vertex;
    dx12::InitialData data{};
    data.pSysMem = packed.data();
    dx12::Buffer* replacement = nullptr;
    if (FAILED(m_device->CreateBuffer(&desc, &data, &replacement)))
        return false;
    Release(m_staticBuffer);
    m_staticBuffer = replacement;
    m_groundRevision = excavation::revision();
    return true;
}

void Dx12Renderer::SetTessellation(int material) {
    bool enabled = ui::graphicsQuality > 0 &&
                   (material == 2 || material == 4 || material == 10 || material == 11);
    m_context->IASetPrimitiveTopology(enabled ? D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST
                                              : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->HSSetShader(enabled ? m_sceneHS : nullptr, nullptr, 0);
    m_context->DSSetShader(enabled ? m_sceneDS : nullptr, nullptr, 0);
}

XMFLOAT4 Dx12Renderer::PbrForGroup(int group) {
    switch (group) {
    case 2:
    case 3:
    case 10:
    case 11:
    case 12:
        return {0.82f, 0.02f, 0, 0};
    case 4:
        return {0.92f, 0, 0, 0};
    case 5:
        return {0.88f, 0, 0, 0};
    case 6:
        return {0.37f, 0.70f, 0, 0};
    case 7:
        return {0.84f, 0.02f, 0, 0};
    case 15:
        return {0.12f, 0.0f, 0, 0};
    case 8:
    case 9:
        return {0.96f, 0, 0, 0};
    case 14:
        return {0.38f, 0, 1.7f, 0};
    default:
        return {0.8f, 0, 0, 0};
    }
}

void Dx12Renderer::DrawInstances(bool shadow, SceneConstants& constants, bool transparent,
                                 int cascade, int localShadow) {
    auto* instanceBuffer = m_dynamicInstances->Get();
    m_context->IASetInputLayout(m_instanceLayout);
    m_context->VSSetShader(m_instanceVS, nullptr, 0);
    const auto& batches = localShadow == 1   ? m_headlightShadowInstanceBatches
                          : localShadow == 2 ? m_streetShadowInstanceBatches
                          : shadow           ? m_shadowInstanceBatches[cascade]
                                             : m_instanceBatches;
    if (!transparent && batches.size() >= 64)
        m_context->beginParallel(m_recordPool.get());
    for (const auto& batch : batches) {
        if (batch.m_mesh->transparent != transparent)
            continue;
        if (shadow && !batch.m_mesh->castsShadow)
            continue;
        const dx11::Mesh* drawn =
            shadow && batch.m_mesh->shadowProxy ? batch.m_mesh->shadowProxy : batch.m_mesh;
        dx12::SamplerState* meshSampler = drawn->wrapTextures ? m_sampler : m_modelSampler;
        m_context->PSSetSamplers(2, 1, &meshSampler);
        // Imported foliage already has dense leaf geometry. Hull/domain
        // tessellation multiplies its cost without improving the silhouette.
        SetTessellation(!drawn->allowTessellation || (drawn->textured && batch.m_material == 4)
                            ? 0
                            : batch.m_material);
        dx12::Buffer* buffers[] = {m_meshCache->VertexBuffer(*drawn), instanceBuffer};
        UINT strides[] = {sizeof(dx11::Vertex), sizeof(InstanceData)}, offsets[] = {0, 0};
        m_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        dx12::Buffer* indexBuffer = m_meshCache->IndexBuffer(*drawn);
        m_context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        const auto& ranges = drawn->materialRanges;
        for (size_t part = 0; part < std::max<size_t>(1, ranges.size()); ++part) {
            const dx11::MaterialRange* range = ranges.empty() ? nullptr : &ranges[part];
            const bool masked = range ? range->alphaTest : drawn->alphaTest;
            dx12::ShaderResourceView* base =
                range ? m_textureLibrary->Find(range->baseFile,
                                               masked ? dx11::texture::Kind::MaskedColor
                                                      : dx11::texture::Kind::Color)
                      : m_textureLibrary->ForMesh(*drawn);
            bool hasModelTexture = base != nullptr;
            constants.m_params.x = float(batch.m_material) + (hasModelTexture ? 0.25f : 0.0f);
            constants.m_temporalInfo.x = batch.m_mesh->temporalStable ? 1.0f : 0.0f;
            constants.m_temporalInfo.y = 0;
            constants.m_materialPbr = PbrForGroup(batch.m_material);
            constants.m_materialSurface = {
                range ? range->clearcoat : 0, range ? range->clearcoatRoughness : 0.1f,
                range ? range->glassIor : 0, drawn->grassFoliage ? 1.0f : 0.0f};
            if (std::strstr(GetCommandLineA(), "--no-clearcoat"))
                constants.m_materialSurface.x = 0;
            constants.m_materialOptions.x = range                   ? range->alphaCutoff
                                            : batch.m_material == 4 ? 0.42f
                                                                    : 0.35f;
            if (range) {
                constants.m_materialPbr.x = range->roughness;
                constants.m_materialPbr.y = range->metallic;
                constants.m_materialPbr.z = range->emissive;
                constants.m_materialPbr.w =
                    float((!range->normalFile.empty() ? 1 : 0) | (!range->ormFile.empty() ? 2 : 0) |
                          (!range->occlusionFile.empty() ? 4 : 0) |
                          (!range->emissiveFile.empty() ? 8 : 0) | (masked ? 16 : 0) |
                          (m_textureLibrary->IsBc5Normal(range->normalFile) ? 32 : 0));
            } else if (batch.m_material != 14 && drawn->textured) {
                constants.m_materialPbr.x = drawn->roughness;
                constants.m_materialPbr.y = drawn->metallic;
            }
            if (!range && masked)
                constants.m_materialPbr.w = 16;
            if (drawn->vehicleWear)
                constants.m_materialPbr.w = float(int(constants.m_materialPbr.w) | 64);
            if (drawn->unlit)
                constants.m_materialPbr.z = -1;
            m_context->UpdateSubresource(m_sceneBuffer, 0, nullptr, &constants, 0, 0);
            if (shadow) {
                bool alpha = masked && hasModelTexture;
                m_context->PSSetShader(alpha ? m_alphaShadowPS : nullptr, nullptr, 0);
                if (alpha)
                    m_context->PSSetShaderResources(0, 1, &base);
            } else {
                int group = batch.m_material;
                dx12::ShaderResourceView* resources[9] = {
                    base,
                    group < dx11::MATERIAL_GROUPS ? m_detailTextures[group] : nullptr,
                    group < dx11::MATERIAL_GROUPS ? m_normalTextures[group] : nullptr,
                    m_detailTextures[9],
                    nullptr,
                    range ? m_textureLibrary->Find(range->normalFile, dx11::texture::Kind::Normal)
                          : nullptr,
                    range ? m_textureLibrary->Find(range->ormFile, dx11::texture::Kind::Linear)
                          : nullptr,
                    range
                        ? m_textureLibrary->Find(range->occlusionFile, dx11::texture::Kind::Linear)
                        : nullptr,
                    range ? m_textureLibrary->Find(range->emissiveFile) : nullptr};
                m_context->PSSetShaderResources(0, 4, resources);
                m_context->PSSetShaderResources(5, 4, resources + 5);
            }
            UINT elementCount =
                range ? range->count
                      : UINT(indexBuffer ? drawn->indices.size() : drawn->vertices.size());
            if (indexBuffer)
                m_context->DrawIndexedInstanced(elementCount, batch.m_count,
                                                range ? range->start : 0, 0, batch.m_start);
            else
                m_context->DrawInstanced(elementCount, batch.m_count, range ? range->start : 0,
                                         batch.m_start);
            ++drawCalls;
            triangleCount += std::uint64_t(elementCount / 3) * batch.m_count;
        }
    }
    m_context->endParallel();
    m_context->PSSetSamplers(2, 1, &m_modelSampler);
}
} // namespace game
