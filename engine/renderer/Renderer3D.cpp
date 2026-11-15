#include "Renderer3D.h"
#include "../rhi/EmbeddedShaders.h"
#include "../core/Log.h"
#include <nvrhi/utils.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

namespace Hargon
{
    static constexpr float kPi = 3.14159265358979323846f;

    glm::mat4 CameraView::GetViewMatrix() const
    {
        glm::vec3 f = glm::length(forward) > 0.0001f ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, 1.0f);
        return glm::lookAtLH(position, position + f, up);
    }

    glm::mat4 CameraView::GetProjectionMatrix() const
    {
        float safeAspect = std::max(0.1f, aspectRatio);
        return glm::perspectiveLH_ZO(glm::radians(fovDegrees), safeAspect, nearClip, farClip);
    }

    glm::mat4 CameraView::GetViewProjectionMatrix() const
    {
        return GetProjectionMatrix() * GetViewMatrix();
    }

    struct alignas(16) SceneCBData
    {
        glm::mat4 viewProj;
        glm::mat4 lightViewProj;
        glm::vec4 cameraPos;       // xyz = pos, w = time
        glm::vec4 dirLightDir;     // xyz = dir toward light, w = intensity
        glm::vec4 dirLightColor;   // xyz = color, w = ambientStrength
        glm::vec4 skyColor;        // xyz = skyHorizonColor, w = fogDensity
        glm::vec4 groundColor;     // xyz = groundAmbientColor, w = pointLightCount
        glm::vec4 pointLightPosRange[8];
        glm::vec4 pointLightColorIntensity[8];
        glm::vec4 envParams0;      // x = tonemapMode, y = exposure, z = white, w = glowIntensity
        glm::vec4 envParams1;      // x = fogHeight, y = fogHeightDensity, z = fogSunScatter, w = contrast
        glm::vec4 envParams2;      // x = brightness, y = saturation, z = glowBloom, w = shadowStrength
        glm::vec4 shadowParams;    // x = shadowBias, y = 1.0/shadowSize, z = shadowsEnabled, w = 0
    };

    struct alignas(16) ShadowCBData
    {
        glm::mat4 lightViewProj;
    };

    // Godot-inspired Fast Frustum Culling (servers/rendering/renderer_scene_cull.h)
    struct FrustumPlane
    {
        glm::vec3 normal{ 0.0f };
        float d = 0.0f;
        uint32_t signs[3]{ 0, 1, 2 }; // indices into bounds[6] for positive-most / negative-most vertex
    };

    struct FrustumCuller
    {
        FrustumPlane planes[6];

        void ExtractFromMatrix(const glm::mat4& vp)
        {
            // Gribb-Hartmann extraction for column-major GLM matrix (row_i = [vp[0][i], vp[1][i], vp[2][i], vp[3][i]])
            auto row = [&](int r) {
                return glm::vec4(vp[0][r], vp[1][r], vp[2][r], vp[3][r]);
            };
            glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
            glm::vec4 raw[6] = {
                r3 + r0, // Left
                r3 - r0, // Right
                r3 + r1, // Bottom
                r3 - r1, // Top
                r2,      // Near (ZO)
                r3 - r2  // Far
            };

            for (int i = 0; i < 6; ++i)
            {
                glm::vec3 n(raw[i].x, raw[i].y, raw[i].z);
                float len = glm::length(n);
                if (len > 1e-6f)
                {
                    planes[i].normal = n / len;
                    planes[i].d = raw[i].w / len;
                }
                // Godot PlaneSign convention: select max corner along normal so if dot(n, maxCorner) + d < 0, AABB is outside
                planes[i].signs[0] = (planes[i].normal.x >= 0.0f) ? 3u : 0u;
                planes[i].signs[1] = (planes[i].normal.y >= 0.0f) ? 4u : 1u;
                planes[i].signs[2] = (planes[i].normal.z >= 0.0f) ? 5u : 2u;
            }
        }

        bool InFrustum(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const
        {
            float bounds[6] = { aabbMin.x, aabbMin.y, aabbMin.z, aabbMax.x, aabbMax.y, aabbMax.z };
            for (int i = 0; i < 6; ++i)
            {
                glm::vec3 p(
                    bounds[planes[i].signs[0]],
                    bounds[planes[i].signs[1]],
                    bounds[planes[i].signs[2]]);
                if (glm::dot(planes[i].normal, p) + planes[i].d < 0.0f)
                    return false;
            }
            return true;
        }
    };

    struct alignas(16) ObjectCBData
    {
        glm::mat4 model;
        glm::mat4 normalMatrix;
        glm::vec4 albedoColor;
        glm::vec4 emissiveColor;   // xyz = emissive, w = strength
        glm::vec4 materialParams;  // x = metallic, y = roughness, z = uvScale, w = patternType
    };

    struct alignas(16) SkyboxCBData
    {
        glm::mat4 viewRotProj;
        glm::vec4 sunDir;          // xyz = sunDir, w = time
        glm::vec4 zenithColor;     // xyz = zenith, w = starIntensity
        glm::vec4 horizonColor;    // xyz = horizon, w = sunSize
    };

    struct alignas(16) LineCBData
    {
        glm::mat4 viewProj;
    };

    Renderer3D::~Renderer3D()
    {
        Shutdown();
    }

    bool Renderer3D::Init(RHIDevice* rhi)
    {
        m_RHI = rhi;
        m_Device = rhi->GetDevice();

        // 1. Compile / create shaders
        m_PBRVS = m_RHI->CreateShader(
            nvrhi::ShaderType::Vertex, "PBRLit_VS", "VSMain",
            g_PBRLit_VS_SPV, sizeof(g_PBRLit_VS_SPV), "assets/shaders/PBRLit.hlsl");
        m_PBRPS = m_RHI->CreateShader(
            nvrhi::ShaderType::Pixel, "PBRLit_PS", "PSMain",
            g_PBRLit_PS_SPV, sizeof(g_PBRLit_PS_SPV), "assets/shaders/PBRLit.hlsl");
        m_ShadowVS = m_RHI->CreateShader(
            nvrhi::ShaderType::Vertex, "ShadowDepth_VS", "VSMain",
            g_ShadowDepth_VS_SPV, sizeof(g_ShadowDepth_VS_SPV), "assets/shaders/ShadowDepth.hlsl");

        m_SkyboxVS = m_RHI->CreateShader(
            nvrhi::ShaderType::Vertex, "Skybox_VS", "VSMain",
            g_Skybox_VS_SPV, sizeof(g_Skybox_VS_SPV), "assets/shaders/Skybox.hlsl");
        m_SkyboxPS = m_RHI->CreateShader(
            nvrhi::ShaderType::Pixel, "Skybox_PS", "PSMain",
            g_Skybox_PS_SPV, sizeof(g_Skybox_PS_SPV), "assets/shaders/Skybox.hlsl");

        m_LineVS = m_RHI->CreateShader(
            nvrhi::ShaderType::Vertex, "LineGizmo_VS", "VSMain",
            g_LineGizmo_VS_SPV, sizeof(g_LineGizmo_VS_SPV), "assets/shaders/LineGizmo.hlsl", Shaders::kLineGizmoHLSL);
        m_LinePS = m_RHI->CreateShader(
            nvrhi::ShaderType::Pixel, "LineGizmo_PS", "PSMain",
            g_LineGizmo_PS_SPV, sizeof(g_LineGizmo_PS_SPV), "assets/shaders/LineGizmo.hlsl", Shaders::kLineGizmoHLSL);

        if (!m_PBRVS || !m_PBRPS || !m_ShadowVS || !m_SkyboxVS || !m_SkyboxPS || !m_LineVS || !m_LinePS)
        {
            Log::Error("Renderer3D failed to create one or more shaders!");
            return false;
        }

        // 2. Input Layouts
        nvrhi::VertexAttributeDesc meshAttrs[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(Vertex3D, position))
                .setElementStride(sizeof(Vertex3D)),
            nvrhi::VertexAttributeDesc()
                .setName("NORMAL")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(Vertex3D, normal))
                .setElementStride(sizeof(Vertex3D)),
            nvrhi::VertexAttributeDesc()
                .setName("TEXCOORD")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setOffset(offsetof(Vertex3D, uv))
                .setElementStride(sizeof(Vertex3D)),
            nvrhi::VertexAttributeDesc()
                .setName("COLOR")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setOffset(offsetof(Vertex3D, color))
                .setElementStride(sizeof(Vertex3D))
        };
        m_MeshInputLayout = m_Device->createInputLayout(meshAttrs, 4, m_PBRVS);

        nvrhi::VertexAttributeDesc lineAttrs[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(LineVertex, position))
                .setElementStride(sizeof(LineVertex)),
            nvrhi::VertexAttributeDesc()
                .setName("COLOR")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setOffset(offsetof(LineVertex, color))
                .setElementStride(sizeof(LineVertex))
        };
        m_LineInputLayout = m_Device->createInputLayout(lineAttrs, 2, m_LineVS);

        // 3. Volatile Constant Buffers
        m_SceneCB  = m_Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(SceneCBData),  "SceneCB",  64));
        m_ObjectCB = m_Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ObjectCBData), "ObjectCB", 2048));
        m_ShadowCB = m_Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ShadowCBData), "ShadowCB", 64));
        m_SkyboxCB = m_Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(SkyboxCBData), "SkyboxCB", 64));
        m_LineCB   = m_Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(LineCBData),   "LineCB",   64));

        // 4. Dynamic Line Vertex Buffers
        nvrhi::BufferDesc lineVbDesc;
        lineVbDesc.byteSize = m_MaxLineVertices * sizeof(LineVertex);
        lineVbDesc.isVertexBuffer = true;
        lineVbDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
        lineVbDesc.keepInitialState = true;
        lineVbDesc.debugName = "DynamicLineVB";
        m_DynamicLineVB = m_Device->createBuffer(lineVbDesc);

        lineVbDesc.debugName = "DynamicGizmoVB";
        m_DynamicGizmoVB = m_Device->createBuffer(lineVbDesc);

        // 5. Default White Texture + Linear Sampler + Built-in Primitive Meshes
        nvrhi::TextureDesc texDesc;
        texDesc.width = 4;
        texDesc.height = 4;
        texDesc.format = nvrhi::Format::RGBA8_UNORM;
        texDesc.debugName = "DefaultWhiteTexture";
        texDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        texDesc.keepInitialState = true;
        m_WhiteTexture = m_Device->createTexture(texDesc);

        nvrhi::SamplerDesc sampDesc;
        sampDesc.setAllFilters(true);
        sampDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Wrap);
        m_LinearSampler = m_Device->createSampler(sampDesc);

        // 5b. 2048x2048 Directional Shadow Map Texture + Framebuffer + Clamp Sampler
        nvrhi::TextureDesc shadowDesc;
        shadowDesc.width = 2048;
        shadowDesc.height = 2048;
        shadowDesc.format = nvrhi::Format::D32;
        shadowDesc.isRenderTarget = true;
        shadowDesc.isShaderResource = true;
        shadowDesc.debugName = "DirectionalShadowMap";
        shadowDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        shadowDesc.keepInitialState = true;
        shadowDesc.useClearValue = true;
        shadowDesc.clearValue = nvrhi::Color(1.0f, 0.0f, 0.0f, 0.0f);
        m_ShadowMapTexture = m_Device->createTexture(shadowDesc);

        nvrhi::FramebufferDesc shadowFbDesc;
        shadowFbDesc.setDepthAttachment(m_ShadowMapTexture);
        m_ShadowFramebuffer = m_Device->createFramebuffer(shadowFbDesc);

        nvrhi::SamplerDesc shadowSampDesc;
        shadowSampDesc.setAllFilters(false);
        shadowSampDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        m_ShadowClampSampler = m_Device->createSampler(shadowSampDesc);

        nvrhi::CommandListHandle initCmd = m_Device->createCommandList();
        initCmd->open();

        nvrhi::utils::ClearDepthStencilAttachment(initCmd, m_ShadowFramebuffer, 1.0f, 0);

        uint32_t whitePixels[16];
        std::fill(std::begin(whitePixels), std::end(whitePixels), 0xFFFFFFFFu);
        initCmd->writeTexture(m_WhiteTexture, 0, 0, whitePixels, 4 * sizeof(uint32_t));

        for (int p = 0; p < static_cast<int>(MeshPrimitive::Count); ++p)
        {
            MeshPrimitive prim = static_cast<MeshPrimitive>(p);
            if (prim == MeshPrimitive::CustomOBJ)
                continue;
            m_PrimitiveMeshes[p] = MeshFactory::CreatePrimitive(m_Device, initCmd, prim);
        }

        initCmd->close();
        m_Device->executeCommandList(initCmd);
        m_RHI->WaitForIdle();

        // 6. Create Binding Sets and Layouts
        {
            nvrhi::BindingSetDesc bset;
            bset.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_SceneCB),
                nvrhi::BindingSetItem::ConstantBuffer(1, m_ObjectCB),
                nvrhi::BindingSetItem::Texture_SRV(0, m_WhiteTexture),
                nvrhi::BindingSetItem::Texture_SRV(1, m_ShadowMapTexture),
                nvrhi::BindingSetItem::Sampler(0, m_LinearSampler),
                nvrhi::BindingSetItem::Sampler(1, m_ShadowClampSampler)
            };
            nvrhi::utils::CreateBindingSetAndLayout(m_Device, nvrhi::ShaderType::All, 0, bset, m_PBRBindingLayout, m_PBRBindingSet);
        }

        {
            nvrhi::BindingSetDesc bset;
            bset.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_ShadowCB),
                nvrhi::BindingSetItem::ConstantBuffer(1, m_ObjectCB)
            };
            nvrhi::utils::CreateBindingSetAndLayout(m_Device, nvrhi::ShaderType::Vertex, 0, bset, m_ShadowBindingLayout, m_ShadowBindingSet);

            nvrhi::GraphicsPipelineDesc desc;
            desc.primType = nvrhi::PrimitiveType::TriangleList;
            desc.inputLayout = m_MeshInputLayout;
            desc.VS = m_ShadowVS;
            desc.PS = nullptr;
            desc.bindingLayouts = { m_ShadowBindingLayout };
            desc.renderState.rasterState
                .setCullNone()
                .setDepthBias(4)
                .setSlopeScaleDepthBias(1.5f);
            desc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthFunc(nvrhi::ComparisonFunc::LessOrEqual);
            m_ShadowPipeline = m_Device->createGraphicsPipeline(desc, m_ShadowFramebuffer);
        }

        {
            nvrhi::BindingSetDesc bset;
            bset.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_SkyboxCB)
            };
            nvrhi::utils::CreateBindingSetAndLayout(m_Device, nvrhi::ShaderType::All, 0, bset, m_SkyboxBindingLayout, m_SkyboxBindingSet);
        }

        {
            nvrhi::BindingSetDesc bset;
            bset.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_LineCB)
            };
            nvrhi::utils::CreateBindingSetAndLayout(m_Device, nvrhi::ShaderType::All, 0, bset, m_LineBindingLayout, m_LineBindingSet);
        }

        m_Particles.reserve(2048);
        Log::RHI("Renderer3D initialized all NVRHI shaders, meshes, and binding layouts.");
        return true;
    }

    void Renderer3D::Shutdown()
    {
        m_PrimitiveMeshes.clear();
        m_CustomMeshes.clear();

        m_ShadowPipeline = nullptr;
        m_PBRPipeline = nullptr;
        m_SkyboxPipeline = nullptr;
        m_LinePipeline = nullptr;
        m_GizmoPipeline = nullptr;

        m_ShadowBindingSet = nullptr;
        m_ShadowBindingLayout = nullptr;
        m_PBRBindingSet = nullptr;
        m_PBRBindingLayout = nullptr;
        m_SkyboxBindingSet = nullptr;
        m_SkyboxBindingLayout = nullptr;
        m_LineBindingSet = nullptr;
        m_LineBindingLayout = nullptr;

        m_ShadowFramebuffer = nullptr;
        m_ShadowMapTexture = nullptr;
        m_ShadowClampSampler = nullptr;
        m_WhiteTexture = nullptr;
        m_LinearSampler = nullptr;

        m_DynamicLineVB = nullptr;
        m_DynamicGizmoVB = nullptr;
        m_SceneCB = nullptr;
        m_ObjectCB = nullptr;
        m_ShadowCB = nullptr;
        m_SkyboxCB = nullptr;
        m_LineCB = nullptr;

        m_MeshInputLayout = nullptr;
        m_LineInputLayout = nullptr;

        m_PBRVS = nullptr;
        m_PBRPS = nullptr;
        m_ShadowVS = nullptr;
        m_SkyboxVS = nullptr;
        m_SkyboxPS = nullptr;
        m_LineVS = nullptr;
        m_LinePS = nullptr;
    }

    void Renderer3D::EnsurePipelines(nvrhi::IFramebuffer* targetFramebuffer)
    {
        const nvrhi::FramebufferInfo& fbInfo = targetFramebuffer->getFramebufferInfo();
        if (m_HasCachedFbInfo && m_CachedFbInfo == fbInfo && m_PBRPipeline)
            return;

        m_CachedFbInfo = fbInfo;
        m_HasCachedFbInfo = true;

        // 1. Skybox Pipeline
        {
            nvrhi::GraphicsPipelineDesc desc;
            desc.primType = nvrhi::PrimitiveType::TriangleList;
            desc.inputLayout = m_MeshInputLayout;
            desc.VS = m_SkyboxVS;
            desc.PS = m_SkyboxPS;
            desc.bindingLayouts = { m_SkyboxBindingLayout };
            desc.renderState.rasterState.setCullNone();
            desc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(false)
                .setDepthFunc(nvrhi::ComparisonFunc::LessOrEqual);
            m_SkyboxPipeline = m_Device->createGraphicsPipeline(desc, targetFramebuffer);
        }

        // 2. PBR Lit Mesh Pipeline
        {
            nvrhi::GraphicsPipelineDesc desc;
            desc.primType = nvrhi::PrimitiveType::TriangleList;
            desc.inputLayout = m_MeshInputLayout;
            desc.VS = m_PBRVS;
            desc.PS = m_PBRPS;
            desc.bindingLayouts = { m_PBRBindingLayout };
            desc.renderState.rasterState.setCullNone();
            desc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthFunc(nvrhi::ComparisonFunc::LessOrEqual);
            m_PBRPipeline = m_Device->createGraphicsPipeline(desc, targetFramebuffer);
        }

        // 3. 3D World Line Pipeline (depth tested)
        {
            nvrhi::GraphicsPipelineDesc desc;
            desc.primType = nvrhi::PrimitiveType::LineList;
            desc.inputLayout = m_LineInputLayout;
            desc.VS = m_LineVS;
            desc.PS = m_LinePS;
            desc.bindingLayouts = { m_LineBindingLayout };
            desc.renderState.rasterState.setCullNone();
            desc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(false)
                .setDepthFunc(nvrhi::ComparisonFunc::LessOrEqual);
            m_LinePipeline = m_Device->createGraphicsPipeline(desc, targetFramebuffer);
        }

        // 4. 3D Gizmo Overlay Pipeline (always on top, depth test disabled)
        {
            nvrhi::GraphicsPipelineDesc desc;
            desc.primType = nvrhi::PrimitiveType::LineList;
            desc.inputLayout = m_LineInputLayout;
            desc.VS = m_LineVS;
            desc.PS = m_LinePS;
            desc.bindingLayouts = { m_LineBindingLayout };
            desc.renderState.rasterState.setCullNone();
            desc.renderState.depthStencilState
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false);
            m_GizmoPipeline = m_Device->createGraphicsPipeline(desc, targetFramebuffer);
        }
    }

    const GPUMesh& Renderer3D::GetOrCreateMesh(nvrhi::ICommandList* cmd, const MeshComponent& meshComp)
    {
        if (meshComp.primitive == MeshPrimitive::CustomOBJ && !meshComp.customObjPath.empty())
        {
            auto it = m_CustomMeshes.find(meshComp.customObjPath);
            if (it != m_CustomMeshes.end())
                return it->second;

            m_CustomMeshes[meshComp.customObjPath] = MeshFactory::LoadOBJ(m_Device, cmd, meshComp.customObjPath);
            return m_CustomMeshes[meshComp.customObjPath];
        }

        int key = static_cast<int>(meshComp.primitive);
        auto it = m_PrimitiveMeshes.find(key);
        if (it != m_PrimitiveMeshes.end())
            return it->second;

        return m_PrimitiveMeshes[0];
    }

    void Renderer3D::UpdateParticles(float dt)
    {
        if (dt <= 0.0f) return;
        for (auto& p : m_Particles)
        {
            p.life += dt;
            p.position += p.velocity * dt;
            p.velocity.y -= 5.5f * dt;
            p.velocity *= std::exp(-2.2f * dt);
        }

        m_Particles.erase(
            std::remove_if(m_Particles.begin(), m_Particles.end(), [](const Particle3D& p) { return p.life >= p.maxLife; }),
            m_Particles.end());
    }

    void Renderer3D::SpawnParticleBurst(const glm::vec3& position, const glm::vec3& color, int count, float speed)
    {
        auto randFloat = [this]() -> float {
            m_RngState ^= m_RngState << 13;
            m_RngState ^= m_RngState >> 17;
            m_RngState ^= m_RngState << 5;
            return static_cast<float>(m_RngState & 0xFFFFu) / 65535.0f;
        };

        for (int i = 0; i < count && m_Particles.size() < 2000; ++i)
        {
            float theta = randFloat() * 2.0f * kPi;
            float phi = (randFloat() - 0.3f) * kPi;
            glm::vec3 dir(
                std::cos(phi) * std::cos(theta),
                std::abs(std::sin(phi)) + 0.25f,
                std::cos(phi) * std::sin(theta));
            dir = glm::normalize(dir);

            float spd = speed * (0.45f + 0.75f * randFloat());
            Particle3D pt;
            pt.position = position;
            pt.velocity = dir * spd;
            pt.color = glm::vec4(color, 1.0f);
            pt.life = 0.0f;
            pt.maxLife = 0.35f + 0.45f * randFloat();
            pt.size = 0.3f + 0.25f * randFloat();
            m_Particles.push_back(pt);
        }
    }

    void Renderer3D::AddTransientLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color)
    {
        if (m_TransientLines.size() + 2 <= m_MaxLineVertices)
        {
            m_TransientLines.push_back({ a, color });
            m_TransientLines.push_back({ b, color });
        }
    }

    void Renderer3D::RegisterCustomMesh(const std::string& key, const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices)
    {
        if (!m_Device || vertices.empty() || indices.empty())
            return;

        GPUMesh mesh;
        mesh.name = key;
        mesh.vertices = vertices;
        mesh.indices = indices;

        nvrhi::CommandListHandle uploadCmd = m_Device->createCommandList();
        uploadCmd->open();
        MeshFactory::UploadMeshBuffers(m_Device, uploadCmd, mesh);
        uploadCmd->close();
        m_Device->executeCommandList(uploadCmd);

        m_CustomMeshes[key] = std::move(mesh);
    }

    void Renderer3D::ClearCustomMeshes()
    {
        if (m_RHI)
            m_RHI->WaitForIdle();
        m_CustomMeshes.clear();
    }

    void Renderer3D::AddLine(std::vector<LineVertex>& lines, const glm::vec3& a, const glm::vec3& b, const glm::vec4& color)
    {
        if (lines.size() + 2 <= m_MaxLineVertices)
        {
            lines.push_back({ a, color });
            lines.push_back({ b, color });
        }
    }

    void Renderer3D::AddWireBox(std::vector<LineVertex>& lines, const glm::vec3& c, const glm::vec3& h, const glm::vec4& color)
    {
        glm::vec3 v[8] = {
            c + glm::vec3(-h.x, -h.y, -h.z),
            c + glm::vec3( h.x, -h.y, -h.z),
            c + glm::vec3( h.x,  h.y, -h.z),
            c + glm::vec3(-h.x,  h.y, -h.z),
            c + glm::vec3(-h.x, -h.y,  h.z),
            c + glm::vec3( h.x, -h.y,  h.z),
            c + glm::vec3( h.x,  h.y,  h.z),
            c + glm::vec3(-h.x,  h.y,  h.z)
        };
        int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };
        for (auto& e : edges)
            AddLine(lines, v[e[0]], v[e[1]], color);
    }

    void Renderer3D::AddWireSphere(std::vector<LineVertex>& lines, const glm::vec3& center, float radius, const glm::vec4& color)
    {
        constexpr int segs = 24;
        for (int i = 0; i < segs; ++i)
        {
            float a0 = (static_cast<float>(i) / segs) * 2.0f * kPi;
            float a1 = (static_cast<float>(i + 1) / segs) * 2.0f * kPi;

            // XZ circle
            AddLine(lines,
                center + glm::vec3(std::cos(a0) * radius, 0.0f, std::sin(a0) * radius),
                center + glm::vec3(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius),
                color);
            // XY circle
            AddLine(lines,
                center + glm::vec3(std::cos(a0) * radius, std::sin(a0) * radius, 0.0f),
                center + glm::vec3(std::cos(a1) * radius, std::sin(a1) * radius, 0.0f),
                color);
            // YZ circle
            AddLine(lines,
                center + glm::vec3(0.0f, std::cos(a0) * radius, std::sin(a0) * radius),
                center + glm::vec3(0.0f, std::cos(a1) * radius, std::sin(a1) * radius),
                color);
        }
    }

    void Renderer3D::BuildGizmoLines(std::vector<LineVertex>& lines, const glm::vec3& center, float scale, GizmoMode mode, int hoveredAxis)
    {
        glm::vec4 colX = (hoveredAxis == 0) ? glm::vec4(1.0f, 0.95f, 0.15f, 1.0f) : glm::vec4(1.0f, 0.22f, 0.22f, 1.0f);
        glm::vec4 colY = (hoveredAxis == 1) ? glm::vec4(1.0f, 0.95f, 0.15f, 1.0f) : glm::vec4(0.22f, 0.95f, 0.28f, 1.0f);
        glm::vec4 colZ = (hoveredAxis == 2) ? glm::vec4(1.0f, 0.95f, 0.15f, 1.0f) : glm::vec4(0.22f, 0.55f, 1.0f, 1.0f);

        auto drawThickShaft = [&](const glm::vec3& axis, const glm::vec3& ortho1, const glm::vec3& ortho2, const glm::vec4& col)
        {
            glm::vec3 tip = center + axis * scale;
            float thickness = scale * 0.018f;
            AddLine(lines, center, tip, col);
            AddLine(lines, center + ortho1 * thickness, tip + ortho1 * thickness, col);
            AddLine(lines, center - ortho1 * thickness, tip - ortho1 * thickness, col);
            AddLine(lines, center + ortho2 * thickness, tip + ortho2 * thickness, col);
            AddLine(lines, center - ortho2 * thickness, tip - ortho2 * thickness, col);

            if (mode == GizmoMode::Translate)
            {
                glm::vec3 headBase = center + axis * (scale * 0.78f);
                float headR = scale * 0.08f;
                for (int i = 0; i < 8; ++i)
                {
                    float a0 = (i / 8.0f) * 2.0f * kPi;
                    float a1 = ((i + 1) / 8.0f) * 2.0f * kPi;
                    glm::vec3 p0 = headBase + (ortho1 * std::cos(a0) + ortho2 * std::sin(a0)) * headR;
                    glm::vec3 p1 = headBase + (ortho1 * std::cos(a1) + ortho2 * std::sin(a1)) * headR;
                    AddLine(lines, tip, p0, col);
                    AddLine(lines, p0, p1, col);
                }
            }
            else if (mode == GizmoMode::Scale)
            {
                AddWireBox(lines, tip, glm::vec3(scale * 0.065f), col);
            }
        };

        if (mode == GizmoMode::Rotate)
        {
            int segs = 36;
            float r = scale * 0.85f;
            for (int i = 0; i < segs; ++i)
            {
                float a0 = (static_cast<float>(i) / segs) * 2.0f * kPi;
                float a1 = (static_cast<float>(i + 1) / segs) * 2.0f * kPi;
                // X rotation ring (YZ plane)
                AddLine(lines,
                    center + glm::vec3(0.0f, std::cos(a0) * r, std::sin(a0) * r),
                    center + glm::vec3(0.0f, std::cos(a1) * r, std::sin(a1) * r), colX);
                // Y rotation ring (XZ plane)
                AddLine(lines,
                    center + glm::vec3(std::cos(a0) * r, 0.0f, std::sin(a0) * r),
                    center + glm::vec3(std::cos(a1) * r, 0.0f, std::sin(a1) * r), colY);
                // Z rotation ring (XY plane)
                AddLine(lines,
                    center + glm::vec3(std::cos(a0) * r, std::sin(a0) * r, 0.0f),
                    center + glm::vec3(std::cos(a1) * r, std::sin(a1) * r, 0.0f), colZ);
            }
        }

        drawThickShaft(glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), colX);
        drawThickShaft(glm::vec3(0, 1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), colY);
        drawThickShaft(glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), colZ);
    }

    int Renderer3D::RaycastGizmo(
        const glm::vec3& gizmoCenter,
        const CameraView& camera,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDir,
        float* outAxisScale) const
    {
        float camDist = glm::length(camera.position - gizmoCenter);
        float gizmoScale = std::clamp(camDist * 0.16f, 1.2f, 6.0f);
        if (outAxisScale)
            *outAxisScale = gizmoScale;

        glm::vec3 axes[3] = {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f}
        };

        int bestAxis = -1;
        float bestDistToAxis = gizmoScale * 0.16f; // pick tolerance radius

        for (int a = 0; a < 3; ++a)
        {
            // Closest distance between ray (rayOrigin + t * rayDir) and segment [gizmoCenter, gizmoCenter + axes[a]*gizmoScale]
            glm::vec3 u = rayDir;
            glm::vec3 v = axes[a] * gizmoScale;
            glm::vec3 w = rayOrigin - gizmoCenter;

            float A = glm::dot(u, u);
            float B = glm::dot(u, v);
            float C = glm::dot(v, v);
            float D = glm::dot(u, w);
            float E = glm::dot(v, w);
            float denom = A * C - B * B;

            float sc = 0.0f, tc = 0.0f;
            if (denom < 1e-5f)
            {
                sc = 0.0f;
                tc = (B > C ? D / B : E / C);
            }
            else
            {
                sc = (B * E - C * D) / denom;
                tc = (A * E - B * D) / denom;
            }

            tc = std::clamp(tc, 0.0f, 1.0f);
            sc = std::max(0.0f, sc);

            glm::vec3 pRay = rayOrigin + u * sc;
            glm::vec3 pSeg = gizmoCenter + v * tc;
            float dist = glm::length(pRay - pSeg);
            if (dist < bestDistToAxis)
            {
                bestDistToAxis = dist;
                bestAxis = a;
            }
        }

        return bestAxis;
    }

    void Renderer3D::RenderScene(
        nvrhi::ICommandList* cmd,
        nvrhi::IFramebuffer* targetFramebuffer,
        const Scene& scene,
        const CameraView& camera,
        float timeSeconds,
        bool drawEditorGrid,
        EntityID selectedEntity,
        GizmoMode gizmoMode,
        int hoveredGizmoAxis,
        bool drawColliders)
    {
        if (!targetFramebuffer)
            return;

        EnsurePipelines(targetFramebuffer);

        m_Stats = {};
        const auto& fbDesc = targetFramebuffer->getDesc();
        uint32_t vpW = fbDesc.colorAttachments[0].texture->getDesc().width;
        uint32_t vpH = fbDesc.colorAttachments[0].texture->getDesc().height;

        nvrhi::ViewportState viewportState = nvrhi::ViewportState()
            .addViewportAndScissorRect(nvrhi::Viewport(static_cast<float>(vpW), static_cast<float>(vpH)));

        // Clear color & depth
        nvrhi::utils::ClearColorAttachment(cmd, targetFramebuffer, 0, nvrhi::Color(0.04f, 0.06f, 0.12f, 1.0f));
        nvrhi::utils::ClearDepthStencilAttachment(cmd, targetFramebuffer, 1.0f, 0);

        // Gather directional & point lights from scene
        glm::vec3 sunDir = glm::normalize(glm::vec3(0.45f, 0.78f, -0.42f));
        glm::vec3 sunColor(1.0f, 0.95f, 0.85f);
        float sunIntensity = 2.5f;
        bool dirCastShadows = true;
        float dirShadowBias = 0.0025f;

        SceneCBData sceneCB{};
        sceneCB.viewProj = camera.GetViewProjectionMatrix();
        sceneCB.cameraPos = glm::vec4(camera.position, timeSeconds);

        int pointLightCount = 0;
        struct PointCandidate
        {
            glm::vec3 pos;
            float range;
            glm::vec3 color;
            float intensity;
            float distSq;
        };
        std::vector<PointCandidate> pointCandidates;
        pointCandidates.reserve(64);

        for (const auto& e : scene.GetEntities())
        {
            if (!e.alive || !e.light.enabled)
                continue;

            if (e.light.isDirectional)
            {
                glm::vec3 fwd = e.transform.GetForward();
                sunDir = glm::normalize(-fwd);
                if (sunDir.y < 0.12f)
                    sunDir = glm::normalize(glm::vec3(sunDir.x, 0.35f, sunDir.z));
                sunColor = e.light.color;
                sunIntensity = e.light.intensity;
                dirCastShadows = e.light.castShadows;
                dirShadowBias = e.light.shadowBias;
            }
            else
            {
                glm::vec3 worldPos = scene.GetWorldPosition(e);
                glm::vec3 diff = worldPos - camera.position;
                float d2 = glm::dot(diff, diff);
                // Prioritize dynamic muzzle flashes / projectiles so they are always active
                if (e.tag.tag == "Projectile" || e.tag.tag == "EnemyProjectile" || e.tag.tag == "MuzzleFlash")
                    d2 *= 0.05f;
                pointCandidates.push_back({ worldPos, e.light.range, e.light.color, e.light.intensity, d2 });
            }
        }

        std::sort(pointCandidates.begin(), pointCandidates.end(),
            [](const PointCandidate& a, const PointCandidate& b) { return a.distSq < b.distSq; });

        for (const auto& pc : pointCandidates)
        {
            if (pointLightCount >= 8)
                break;
            sceneCB.pointLightPosRange[pointLightCount] = glm::vec4(pc.pos, pc.range);
            sceneCB.pointLightColorIntensity[pointLightCount] = glm::vec4(pc.color, pc.intensity);
            ++pointLightCount;
        }

        // 0. Real-Time 2048x2048 Directional Shadow Depth Pass (Godot DirectionalLight3D-inspired)
        bool shadowsOn = scene.worldEnv.shadowsEnabled && dirCastShadows && m_ShadowPipeline && m_ShadowFramebuffer;
        glm::mat4 lightViewProj(1.0f);
        if (shadowsOn)
        {
            glm::vec3 flatFwd(camera.forward.x, 0.0f, camera.forward.z);
            float ffLen = glm::length(flatFwd);
            glm::vec3 focus = camera.position + (ffLen > 0.05f ? (flatFwd / ffLen) * 14.0f : glm::vec3(0.0f));
            focus.y = 0.0f;

            float orthoHalf = std::clamp(scene.arenaHalfSize * 1.1f, 28.0f, 44.0f);
            glm::vec3 lightUp = (std::abs(sunDir.y) > 0.98f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
            glm::mat4 tmpView = glm::lookAtLH(focus + sunDir * 55.0f, focus, lightUp);
            float texelWorld = (2.0f * orthoHalf) / 2048.0f;
            glm::vec3 rightAxis(tmpView[0][0], tmpView[1][0], tmpView[2][0]);
            glm::vec3 upAxis(tmpView[0][1], tmpView[1][1], tmpView[2][1]);
            float sx = std::floor(glm::dot(focus, rightAxis) / texelWorld + 0.5f) * texelWorld;
            float sy = std::floor(glm::dot(focus, upAxis) / texelWorld + 0.5f) * texelWorld;
            glm::vec3 snappedFocus = rightAxis * sx + upAxis * sy + sunDir * glm::dot(focus, sunDir);

            glm::mat4 lightView = glm::lookAtLH(snappedFocus + sunDir * 55.0f, snappedFocus, lightUp);
            glm::mat4 lightProj = glm::orthoLH_ZO(-orthoHalf, orthoHalf, -orthoHalf, orthoHalf, 1.0f, 130.0f);
            lightViewProj = lightProj * lightView;

            ShadowCBData shadowCB{ lightViewProj };
            cmd->writeBuffer(m_ShadowCB, &shadowCB, sizeof(shadowCB));
            nvrhi::utils::ClearDepthStencilAttachment(cmd, m_ShadowFramebuffer, 1.0f, 0);

            nvrhi::ViewportState shadowVp = nvrhi::ViewportState()
                .addViewportAndScissorRect(nvrhi::Viewport(2048.0f, 2048.0f));

            for (const auto& e : scene.GetEntities())
            {
                if (!e.alive || !e.mesh.enabled) continue;
                if (e.tag.tag == "ViewModel" || e.tag.tag == "MuzzleFlash" || e.tag.tag == "Projectile" || e.tag.tag == "EnemyProjectile")
                    continue;

                const GPUMesh& gpuMesh = GetOrCreateMesh(cmd, e.mesh);
                if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0)
                    continue;

                ObjectCBData objCB{};
                objCB.model = scene.GetWorldMatrix(e);
                cmd->writeBuffer(m_ObjectCB, &objCB, sizeof(objCB));

                nvrhi::GraphicsState state;
                state.pipeline = m_ShadowPipeline;
                state.framebuffer = m_ShadowFramebuffer;
                state.viewport = shadowVp;
                state.bindings = { m_ShadowBindingSet };
                state.vertexBuffers = { { gpuMesh.vertexBuffer, 0, 0 } };
                state.indexBuffer = { gpuMesh.indexBuffer, nvrhi::Format::R32_UINT, 0 };
                cmd->setGraphicsState(state);

                nvrhi::DrawArguments args;
                args.vertexCount = gpuMesh.indexCount;
                cmd->drawIndexed(args);
                ++m_Stats.drawCalls;
            }
        }

        sceneCB.lightViewProj = lightViewProj;
        sceneCB.dirLightDir = glm::vec4(sunDir, sunIntensity);
        sceneCB.dirLightColor = glm::vec4(sunColor, scene.ambientIntensity);
        sceneCB.skyColor = glm::vec4(scene.skyHorizonColor, scene.fogDensity);
        sceneCB.groundColor = glm::vec4(scene.groundAmbientColor, static_cast<float>(pointLightCount));
        sceneCB.envParams0 = glm::vec4(
            static_cast<float>(scene.worldEnv.tonemapMode),
            scene.worldEnv.tonemapExposure,
            scene.worldEnv.tonemapWhite,
            scene.worldEnv.glowEnabled ? scene.worldEnv.glowIntensity : 0.0f);
        sceneCB.envParams1 = glm::vec4(
            scene.worldEnv.fogHeight,
            scene.worldEnv.fogHeightDensity,
            scene.worldEnv.fogSunScatter,
            scene.worldEnv.adjustmentEnabled ? scene.worldEnv.adjustmentContrast : 1.0f);
        sceneCB.envParams2 = glm::vec4(
            scene.worldEnv.adjustmentEnabled ? scene.worldEnv.adjustmentBrightness : 1.0f,
            scene.worldEnv.adjustmentEnabled ? scene.worldEnv.adjustmentSaturation : 1.0f,
            scene.worldEnv.glowEnabled ? scene.worldEnv.glowBloom : 0.0f,
            0.70f);
        sceneCB.shadowParams = glm::vec4(
            dirShadowBias,
            1.0f / 2048.0f,
            shadowsOn ? 1.0f : 0.0f,
            0.0f);
        m_Stats.activePointLights = static_cast<uint32_t>(pointLightCount);

        // 1. Render Skybox
        {
            SkyboxCBData skyCB{};
            glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(camera.GetViewMatrix()));
            skyCB.viewRotProj = camera.GetProjectionMatrix() * viewNoTranslation;
            skyCB.sunDir = glm::vec4(sunDir, timeSeconds);
            skyCB.zenithColor = glm::vec4(scene.skyZenithColor, 1.0f);
            skyCB.horizonColor = glm::vec4(scene.skyHorizonColor, 1.0f);

            cmd->writeBuffer(m_SkyboxCB, &skyCB, sizeof(skyCB));

            const GPUMesh& cubeMesh = m_PrimitiveMeshes[static_cast<int>(MeshPrimitive::Cube)];
            nvrhi::GraphicsState state;
            state.pipeline = m_SkyboxPipeline;
            state.framebuffer = targetFramebuffer;
            state.viewport = viewportState;
            state.bindings = { m_SkyboxBindingSet };
            state.vertexBuffers = { { cubeMesh.vertexBuffer, 0, 0 } };
            state.indexBuffer = { cubeMesh.indexBuffer, nvrhi::Format::R32_UINT, 0 };

            cmd->setGraphicsState(state);
            nvrhi::DrawArguments args;
            args.vertexCount = cubeMesh.indexCount;
            cmd->drawIndexed(args);
            ++m_Stats.drawCalls;
        }

        // 2. Render 3D PBR Lit Entities (with Godot PlaneSign Frustum Culling & Hierarchical World Transforms)
        cmd->writeBuffer(m_SceneCB, &sceneCB, sizeof(sceneCB));

        FrustumCuller frustum;
        frustum.ExtractFromMatrix(sceneCB.viewProj);

        for (const auto& e : scene.GetEntities())
        {
            if (!e.alive) continue;
            ++m_Stats.entityCount;

            if (!e.mesh.enabled)
                continue;

            const GPUMesh& gpuMesh = GetOrCreateMesh(cmd, e.mesh);
            if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0)
                continue;

            glm::mat4 worldMat = scene.GetWorldMatrix(e);

            // Perform fast frustum culling for bounded entities (skip global level mesh & weapon viewmodel)
            if (scene.worldEnv.frustumCullingEnabled &&
                e.id != selectedEntity &&
                e.tag.tag != "LevelGeometry" &&
                e.tag.tag != "ViewModel")
            {
                glm::vec3 worldPos = glm::vec3(worldMat[3]);
                glm::vec3 halfExt = glm::max(glm::abs(e.transform.scale) * 1.35f, glm::vec3(1.2f));
                if (!frustum.InFrustum(worldPos - halfExt, worldPos + halfExt))
                {
                    ++m_Stats.culledEntities;
                    continue;
                }
            }

            ObjectCBData objCB{};
            objCB.model = worldMat;
            objCB.normalMatrix = glm::transpose(glm::inverse(objCB.model));
            objCB.albedoColor = e.material.albedoColor;
            objCB.emissiveColor = glm::vec4(e.material.emissiveColor, e.material.emissiveStrength);
            objCB.materialParams = glm::vec4(
                e.material.metallic,
                e.material.roughness,
                e.material.uvScale,
                static_cast<float>(e.material.patternType));

            cmd->writeBuffer(m_ObjectCB, &objCB, sizeof(objCB));

            nvrhi::GraphicsState state;
            state.pipeline = m_PBRPipeline;
            state.framebuffer = targetFramebuffer;
            state.viewport = viewportState;
            state.bindings = { m_PBRBindingSet };
            state.vertexBuffers = { { gpuMesh.vertexBuffer, 0, 0 } };
            state.indexBuffer = { gpuMesh.indexBuffer, nvrhi::Format::R32_UINT, 0 };

            cmd->setGraphicsState(state);
            nvrhi::DrawArguments args;
            args.vertexCount = gpuMesh.indexCount;
            cmd->drawIndexed(args);

            ++m_Stats.drawCalls;
            m_Stats.triangleCount += gpuMesh.indexCount / 3;
        }

        // 3. Render 3D Lines (Editor Grid, Selection Highlight, Colliders, Particles)
        std::vector<LineVertex> worldLines;
        worldLines.reserve(2048);

        if (drawEditorGrid)
        {
            int gridExtent = 36;
            for (int i = -gridExtent; i <= gridExtent; i += 2)
            {
                float f = static_cast<float>(i);
                glm::vec4 col = (i == 0)
                    ? glm::vec4(0.35f, 0.75f, 1.0f, 0.85f)
                    : ((i % 10 == 0) ? glm::vec4(0.25f, 0.45f, 0.65f, 0.55f) : glm::vec4(0.18f, 0.25f, 0.35f, 0.35f));
                AddLine(worldLines, glm::vec3(f, 0.02f, -gridExtent), glm::vec3(f, 0.02f, gridExtent), col);
                AddLine(worldLines, glm::vec3(-gridExtent, 0.02f, f), glm::vec3(gridExtent, 0.02f, f), col);
            }
        }

        if (drawColliders)
        {
            for (const auto& e : scene.GetEntities())
            {
                if (!e.alive || !e.collider.enabled) continue;
                glm::vec4 col = e.collider.isTrigger ? glm::vec4(1.0f, 0.85f, 0.2f, 1.0f) : glm::vec4(0.2f, 1.0f, 0.4f, 1.0f);
                if (e.collider.isSphere)
                    AddWireSphere(worldLines, e.transform.position, e.collider.sphereRadius, col);
                else
                    AddWireBox(worldLines, e.transform.position, e.collider.boxHalfExtents, col);
            }
        }

        // Particles rendered as glowing 3D star/streak bursts
        m_Stats.activeParticles = static_cast<uint32_t>(m_Particles.size());
        for (const auto& p : m_Particles)
        {
            float alpha = 1.0f - (p.life / std::max(p.maxLife, 0.01f));
            glm::vec4 col(p.color.r * alpha, p.color.g * alpha, p.color.b * alpha, 1.0f);
            glm::vec3 tail = p.position - p.velocity * 0.055f;
            AddLine(worldLines, p.position, tail, col);
            float s = p.size * alpha * 0.35f;
            AddLine(worldLines, p.position - glm::vec3(s, 0, 0), p.position + glm::vec3(s, 0, 0), col);
            AddLine(worldLines, p.position - glm::vec3(0, s, 0), p.position + glm::vec3(0, s, 0), col);
            AddLine(worldLines, p.position - glm::vec3(0, 0, s), p.position + glm::vec3(0, 0, s), col);
        }

        // Selection wireframe box
        const Entity* sel = scene.GetEntity(selectedEntity);
        if (sel)
        {
            glm::vec3 halfExt = glm::max(sel->transform.scale * 0.58f, glm::vec3(0.4f));
            AddWireBox(worldLines, sel->transform.position, halfExt, glm::vec4(1.0f, 0.78f, 0.15f, 1.0f));
            if (sel->light.enabled && !sel->light.isDirectional)
            {
                AddWireSphere(worldLines, sel->transform.position, sel->light.range, glm::vec4(sel->light.color, 0.7f));
            }
        }

        // Append transient lines (e.g. Thunderbolt lightning beams, shotgun tracers)
        for (const auto& tl : m_TransientLines)
        {
            if (worldLines.size() < m_MaxLineVertices)
                worldLines.push_back(tl);
        }
        m_TransientLines.clear();

        LineCBData lineCB{};
        lineCB.viewProj = camera.GetViewProjectionMatrix();
        cmd->writeBuffer(m_LineCB, &lineCB, sizeof(lineCB));

        if (!worldLines.empty())
        {
            cmd->writeBuffer(m_DynamicLineVB, worldLines.data(), worldLines.size() * sizeof(LineVertex));

            nvrhi::GraphicsState state;
            state.pipeline = m_LinePipeline;
            state.framebuffer = targetFramebuffer;
            state.viewport = viewportState;
            state.bindings = { m_LineBindingSet };
            state.vertexBuffers = { { m_DynamicLineVB, 0, 0 } };

            cmd->setGraphicsState(state);
            nvrhi::DrawArguments args;
            args.vertexCount = static_cast<uint32_t>(worldLines.size());
            cmd->draw(args);
            ++m_Stats.drawCalls;
        }

        // 4. Render 3D Transform Gizmo on top of selected entity
        if (sel)
        {
            std::vector<LineVertex> gizmoLines;
            gizmoLines.reserve(512);
            float camDist = glm::length(camera.position - sel->transform.position);
            float gizmoScale = std::clamp(camDist * 0.16f, 1.2f, 6.0f);
            BuildGizmoLines(gizmoLines, sel->transform.position, gizmoScale, gizmoMode, hoveredGizmoAxis);

            if (!gizmoLines.empty())
            {
                cmd->writeBuffer(m_DynamicGizmoVB, gizmoLines.data(), gizmoLines.size() * sizeof(LineVertex));

                nvrhi::GraphicsState state;
                state.pipeline = m_GizmoPipeline;
                state.framebuffer = targetFramebuffer;
                state.viewport = viewportState;
                state.bindings = { m_LineBindingSet };
                state.vertexBuffers = { { m_DynamicGizmoVB, 0, 0 } };

                cmd->setGraphicsState(state);
                nvrhi::DrawArguments args;
                args.vertexCount = static_cast<uint32_t>(gizmoLines.size());
                cmd->draw(args);
                ++m_Stats.drawCalls;
            }
        }
    }
}
