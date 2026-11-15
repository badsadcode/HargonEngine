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
        glm::mat4 lightViewProj0;
        glm::mat4 lightViewProj1;
        glm::vec4 cameraPos;                  // xyz = pos, w = time
        glm::vec4 dirLightDir;                // xyz = dir toward light, w = intensity
        glm::vec4 dirLightColor;              // xyz = color, w = ambientStrength
        glm::vec4 skyColor;                   // xyz = skyHorizonColor, w = fogDensity
        glm::vec4 groundColor;                // xyz = groundAmbientColor, w = pointLightCount
        glm::vec4 pointLightPosRange[8];
        glm::vec4 pointLightColorIntensity[8];
        glm::vec4 pointLightShadowInfo[8];    // x = shadowSlot (-1 none, 0..3), y = nearClip, z = shadowBias, w = shadowOpacity
        glm::vec4 envParams0;                 // x = tonemapMode, y = exposure, z = white, w = glowIntensity
        glm::vec4 envParams1;                 // x = fogHeight, y = fogHeightDensity, z = fogSunScatter, w = contrast
        glm::vec4 envParams2;                 // x = brightness, y = saturation, z = glowBloom, w = dirShadowOpacity
        glm::vec4 shadowParams;               // x = dirShadowBias, y = dirNormalBias, z = dirShadowBlur, w = shadowsEnabled
        glm::vec4 ssaoParams;                 // x = ssaoEnabled, y = ssaoDirectAffect, z = invScreenW, w = invScreenH
    };

    struct alignas(16) ShadowCBData
    {
        glm::mat4 lightViewProj;
    };

    struct alignas(16) SSAOCBData
    {
        glm::mat4 proj;
        glm::mat4 invProj;
        glm::vec4 ssaoParams;                 // x = radius, y = intensity, z = bias, w = enabled
        glm::vec4 viewportInfo;               // x = invW, y = invH, z = time, w = 0
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

        m_SSAOVS = m_RHI->CreateShader(
            nvrhi::ShaderType::Vertex, "SSAO_VS", "VSMain",
            g_SSAO_VS_SPV, sizeof(g_SSAO_VS_SPV), "assets/shaders/SSAO.hlsl");
        m_SSAOPS = m_RHI->CreateShader(
            nvrhi::ShaderType::Pixel, "SSAO_PS", "PSMain",
            g_SSAO_PS_SPV, sizeof(g_SSAO_PS_SPV), "assets/shaders/SSAO.hlsl");
        m_SSAOBlurPS = m_RHI->CreateShader(
            nvrhi::ShaderType::Pixel, "SSAOBlur_PS", "PSBlur",
            g_SSAO_Blur_PS_SPV, sizeof(g_SSAO_Blur_PS_SPV), "assets/shaders/SSAO.hlsl");

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

        if (!m_PBRVS || !m_PBRPS || !m_ShadowVS || !m_SSAOVS || !m_SSAOPS || !m_SSAOBlurPS ||
            !m_SkyboxVS || !m_SkyboxPS || !m_LineVS || !m_LinePS)
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

        // 3. Volatile Constant Buffers (sized generously for 2-cascade PSSM + 4x6 Omni faces + Z-Prepass)
        m_SceneCB  = m_Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(SceneCBData),  "SceneCB",  64));
        m_ObjectCB = m_Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ObjectCBData), "ObjectCB", 16384));
        m_ShadowCB = m_Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ShadowCBData), "ShadowCB", 512));
        m_SSAOCB   = m_Device->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(SSAOCBData),   "SSAOCB",   64));
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

        // 5b. 2048x1024 2-Cascade Directional PSSM + 3072x1024 8-Slot 6-Face OmniShadowAtlas
        nvrhi::TextureDesc shadowDesc;
        shadowDesc.width = 2048;
        shadowDesc.height = 1024;
        shadowDesc.format = nvrhi::Format::D32;
        shadowDesc.isRenderTarget = true;
        shadowDesc.isShaderResource = true;
        shadowDesc.isTypeless = true;
        shadowDesc.debugName = "DirectionalShadowMapPSSM";
        shadowDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        shadowDesc.keepInitialState = true;
        shadowDesc.useClearValue = true;
        shadowDesc.clearValue = nvrhi::Color(1.0f, 0.0f, 0.0f, 0.0f);
        m_ShadowMapTexture = m_Device->createTexture(shadowDesc);

        nvrhi::FramebufferDesc shadowFbDesc;
        shadowFbDesc.setDepthAttachment(m_ShadowMapTexture);
        m_ShadowFramebuffer = m_Device->createFramebuffer(shadowFbDesc);

        nvrhi::TextureDesc omniDesc;
        omniDesc.width = 3072;
        omniDesc.height = 1024;
        omniDesc.format = nvrhi::Format::D32;
        omniDesc.isRenderTarget = true;
        omniDesc.isShaderResource = true;
        omniDesc.isTypeless = true;
        omniDesc.debugName = "OmniPointShadowAtlas";
        omniDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        omniDesc.keepInitialState = true;
        omniDesc.useClearValue = true;
        omniDesc.clearValue = nvrhi::Color(1.0f, 0.0f, 0.0f, 0.0f);
        m_OmniShadowAtlas = m_Device->createTexture(omniDesc);

        nvrhi::FramebufferDesc omniFbDesc;
        omniFbDesc.setDepthAttachment(m_OmniShadowAtlas);
        m_OmniShadowFramebuffer = m_Device->createFramebuffer(omniFbDesc);

        nvrhi::SamplerDesc shadowSampDesc;
        shadowSampDesc.setAllFilters(false);
        shadowSampDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        m_ShadowClampSampler = m_Device->createSampler(shadowSampDesc);

        nvrhi::SamplerDesc pointSampDesc;
        pointSampDesc.setAllFilters(false);
        pointSampDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        m_PointClampSampler = m_Device->createSampler(pointSampDesc);

        // Allocate initial Camera Depth Prepass + Half-Res SSAO targets before initCmd clear
        EnsurePrepassResources(1280, 720);

        nvrhi::CommandListHandle initCmd = m_Device->createCommandList();
        initCmd->open();

        nvrhi::utils::ClearDepthStencilAttachment(initCmd, m_ShadowFramebuffer, 1.0f, 0);
        nvrhi::utils::ClearDepthStencilAttachment(initCmd, m_OmniShadowFramebuffer, 1.0f, 0);
        if (m_PrepassFramebuffer)
            nvrhi::utils::ClearDepthStencilAttachment(initCmd, m_PrepassFramebuffer, 1.0f, 0);
        if (m_SSAOFramebuffer)
            nvrhi::utils::ClearColorAttachment(initCmd, m_SSAOFramebuffer, 0, nvrhi::Color(1.0f, 1.0f, 1.0f, 1.0f));
        if (m_SSAOBlurFramebuffer)
            nvrhi::utils::ClearColorAttachment(initCmd, m_SSAOBlurFramebuffer, 0, nvrhi::Color(1.0f, 1.0f, 1.0f, 1.0f));

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
                nvrhi::BindingSetItem::Texture_SRV(2, m_OmniShadowAtlas),
                nvrhi::BindingSetItem::Texture_SRV(3, m_SSAOBlurTexture),
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

            // Unbiased Depth Prepass pipeline for crisp camera Z-Prepass -> SSAO
            nvrhi::GraphicsPipelineDesc preDesc;
            preDesc.primType = nvrhi::PrimitiveType::TriangleList;
            preDesc.inputLayout = m_MeshInputLayout;
            preDesc.VS = m_ShadowVS;
            preDesc.PS = nullptr;
            preDesc.bindingLayouts = { m_ShadowBindingLayout };
            preDesc.renderState.rasterState.setCullNone();
            preDesc.renderState.depthStencilState
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthFunc(nvrhi::ComparisonFunc::LessOrEqual);
            m_DepthPrepassPipeline = m_Device->createGraphicsPipeline(preDesc, m_PrepassFramebuffer);
        }

        {
            nvrhi::BindingSetDesc bset;
            bset.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_SSAOCB),
                nvrhi::BindingSetItem::Texture_SRV(0, m_PrepassDepthTexture),
                nvrhi::BindingSetItem::Sampler(0, m_PointClampSampler)
            };
            nvrhi::utils::CreateBindingSetAndLayout(m_Device, nvrhi::ShaderType::Pixel, 0, bset, m_SSAOBindingLayout, m_SSAOBindingSet);

            nvrhi::GraphicsPipelineDesc desc;
            desc.primType = nvrhi::PrimitiveType::TriangleList;
            desc.inputLayout = nullptr;
            desc.VS = m_SSAOVS;
            desc.PS = m_SSAOPS;
            desc.bindingLayouts = { m_SSAOBindingLayout };
            desc.renderState.rasterState.setCullNone();
            desc.renderState.depthStencilState
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false);
            m_SSAOPipeline = m_Device->createGraphicsPipeline(desc, m_SSAOFramebuffer);
        }

        {
            nvrhi::BindingSetDesc bset;
            bset.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_SSAOCB),
                nvrhi::BindingSetItem::Texture_SRV(1, m_SSAOTexture),
                nvrhi::BindingSetItem::Sampler(0, m_PointClampSampler)
            };
            nvrhi::utils::CreateBindingSetAndLayout(m_Device, nvrhi::ShaderType::Pixel, 0, bset, m_SSAOBlurBindingLayout, m_SSAOBlurBindingSet);

            nvrhi::GraphicsPipelineDesc desc;
            desc.primType = nvrhi::PrimitiveType::TriangleList;
            desc.inputLayout = nullptr;
            desc.VS = m_SSAOVS;
            desc.PS = m_SSAOBlurPS;
            desc.bindingLayouts = { m_SSAOBlurBindingLayout };
            desc.renderState.rasterState.setCullNone();
            desc.renderState.depthStencilState
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false);
            m_SSAOBlurPipeline = m_Device->createGraphicsPipeline(desc, m_SSAOBlurFramebuffer);
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
        m_DepthPrepassPipeline = nullptr;
        m_SSAOPipeline = nullptr;
        m_SSAOBlurPipeline = nullptr;
        m_PBRPipeline = nullptr;
        m_SkyboxPipeline = nullptr;
        m_LinePipeline = nullptr;
        m_GizmoPipeline = nullptr;

        m_ShadowBindingSet = nullptr;
        m_ShadowBindingLayout = nullptr;
        m_SSAOBindingSet = nullptr;
        m_SSAOBindingLayout = nullptr;
        m_SSAOBlurBindingSet = nullptr;
        m_SSAOBlurBindingLayout = nullptr;
        m_PBRBindingSet = nullptr;
        m_PBRBindingLayout = nullptr;
        m_SkyboxBindingSet = nullptr;
        m_SkyboxBindingLayout = nullptr;
        m_LineBindingSet = nullptr;
        m_LineBindingLayout = nullptr;

        m_ShadowFramebuffer = nullptr;
        m_ShadowMapTexture = nullptr;
        m_OmniShadowFramebuffer = nullptr;
        m_OmniShadowAtlas = nullptr;
        m_PrepassFramebuffer = nullptr;
        m_PrepassDepthTexture = nullptr;
        m_SSAOFramebuffer = nullptr;
        m_SSAOTexture = nullptr;
        m_SSAOBlurFramebuffer = nullptr;
        m_SSAOBlurTexture = nullptr;
        m_PrepassWidth = 0;
        m_PrepassHeight = 0;
        m_ShadowClampSampler = nullptr;
        m_PointClampSampler = nullptr;
        m_WhiteTexture = nullptr;
        m_LinearSampler = nullptr;

        m_DynamicLineVB = nullptr;
        m_DynamicGizmoVB = nullptr;
        m_SceneCB = nullptr;
        m_ObjectCB = nullptr;
        m_ShadowCB = nullptr;
        m_SSAOCB = nullptr;
        m_SkyboxCB = nullptr;
        m_LineCB = nullptr;

        m_MeshInputLayout = nullptr;
        m_LineInputLayout = nullptr;

        m_PBRVS = nullptr;
        m_PBRPS = nullptr;
        m_ShadowVS = nullptr;
        m_SSAOVS = nullptr;
        m_SSAOPS = nullptr;
        m_SSAOBlurPS = nullptr;
        m_SkyboxVS = nullptr;
        m_SkyboxPS = nullptr;
        m_LineVS = nullptr;
        m_LinePS = nullptr;
    }

    void Renderer3D::EnsurePrepassResources(uint32_t vpW, uint32_t vpH)
    {
        vpW = std::max(vpW, 64u);
        vpH = std::max(vpH, 64u);
        if (m_PrepassWidth == vpW && m_PrepassHeight == vpH && m_PrepassDepthTexture && m_SSAOBlurTexture)
            return;

        if (m_PrepassWidth > 0 && m_RHI)
            m_RHI->WaitForIdle();

        m_PrepassWidth = vpW;
        m_PrepassHeight = vpH;
        uint32_t halfW = std::max(vpW / 2u, 32u);
        uint32_t halfH = std::max(vpH / 2u, 32u);

        m_PrepassFramebuffer = nullptr;
        m_PrepassDepthTexture = nullptr;
        m_SSAOFramebuffer = nullptr;
        m_SSAOTexture = nullptr;
        m_SSAOBlurFramebuffer = nullptr;
        m_SSAOBlurTexture = nullptr;

        nvrhi::TextureDesc depthDesc;
        depthDesc.width = vpW;
        depthDesc.height = vpH;
        depthDesc.format = nvrhi::Format::D32;
        depthDesc.isRenderTarget = true;
        depthDesc.isShaderResource = true;
        depthDesc.isTypeless = true;
        depthDesc.debugName = "PrepassDepthTex";
        depthDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        depthDesc.keepInitialState = true;
        depthDesc.useClearValue = true;
        depthDesc.clearValue = nvrhi::Color(1.0f, 0.0f, 0.0f, 0.0f);
        m_PrepassDepthTexture = m_Device->createTexture(depthDesc);

        nvrhi::FramebufferDesc depthFb;
        depthFb.setDepthAttachment(m_PrepassDepthTexture);
        m_PrepassFramebuffer = m_Device->createFramebuffer(depthFb);

        nvrhi::TextureDesc ssaoDesc;
        ssaoDesc.width = halfW;
        ssaoDesc.height = halfH;
        ssaoDesc.format = nvrhi::Format::RGBA8_UNORM;
        ssaoDesc.isRenderTarget = true;
        ssaoDesc.isShaderResource = true;
        ssaoDesc.debugName = "SSAORawTex";
        ssaoDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        ssaoDesc.keepInitialState = true;
        ssaoDesc.useClearValue = true;
        ssaoDesc.clearValue = nvrhi::Color(1.0f, 1.0f, 1.0f, 1.0f);
        m_SSAOTexture = m_Device->createTexture(ssaoDesc);

        nvrhi::FramebufferDesc ssaoFb;
        ssaoFb.addColorAttachment(m_SSAOTexture);
        m_SSAOFramebuffer = m_Device->createFramebuffer(ssaoFb);

        ssaoDesc.debugName = "SSAOBlurTex";
        m_SSAOBlurTexture = m_Device->createTexture(ssaoDesc);
        nvrhi::FramebufferDesc blurFb;
        blurFb.addColorAttachment(m_SSAOBlurTexture);
        m_SSAOBlurFramebuffer = m_Device->createFramebuffer(blurFb);

        // Rebuild binding sets if layouts already exist
        if (m_SSAOBindingLayout)
        {
            nvrhi::BindingSetDesc bset;
            bset.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_SSAOCB),
                nvrhi::BindingSetItem::Texture_SRV(0, m_PrepassDepthTexture),
                nvrhi::BindingSetItem::Sampler(0, m_PointClampSampler)
            };
            m_SSAOBindingSet = m_Device->createBindingSet(bset, m_SSAOBindingLayout);
        }
        if (m_SSAOBlurBindingLayout)
        {
            nvrhi::BindingSetDesc bset;
            bset.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_SSAOCB),
                nvrhi::BindingSetItem::Texture_SRV(1, m_SSAOTexture),
                nvrhi::BindingSetItem::Sampler(0, m_PointClampSampler)
            };
            m_SSAOBlurBindingSet = m_Device->createBindingSet(bset, m_SSAOBlurBindingLayout);
        }
        if (m_PBRBindingLayout)
        {
            nvrhi::BindingSetDesc bset;
            bset.bindings = {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_SceneCB),
                nvrhi::BindingSetItem::ConstantBuffer(1, m_ObjectCB),
                nvrhi::BindingSetItem::Texture_SRV(0, m_WhiteTexture),
                nvrhi::BindingSetItem::Texture_SRV(1, m_ShadowMapTexture),
                nvrhi::BindingSetItem::Texture_SRV(2, m_OmniShadowAtlas),
                nvrhi::BindingSetItem::Texture_SRV(3, m_SSAOBlurTexture),
                nvrhi::BindingSetItem::Sampler(0, m_LinearSampler),
                nvrhi::BindingSetItem::Sampler(1, m_ShadowClampSampler)
            };
            m_PBRBindingSet = m_Device->createBindingSet(bset, m_PBRBindingLayout);
        }
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
        m_Particles.clear();
        m_TransientLines.clear();
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
        EnsurePrepassResources(vpW, vpH);

        nvrhi::ViewportState viewportState = nvrhi::ViewportState()
            .addViewportAndScissorRect(nvrhi::Viewport(static_cast<float>(vpW), static_cast<float>(vpH)));

        // Gather directional & point lights from scene
        glm::vec3 sunDir = glm::normalize(glm::vec3(0.45f, 0.78f, -0.42f));
        glm::vec3 sunColor(1.0f, 0.95f, 0.85f);
        float sunIntensity = 2.5f;
        bool dirCastShadows = true;
        float dirShadowBias = 0.0020f;
        float dirShadowOpacity = 0.94f;
        float dirNormalBias = 0.035f;
        float dirShadowBlur = 1.25f;
        float dirShadowMaxDist = 85.0f;

        SceneCBData sceneCB{};
        sceneCB.viewProj = camera.GetViewProjectionMatrix();
        sceneCB.cameraPos = glm::vec4(camera.position, timeSeconds);
        for (int i = 0; i < 8; ++i)
            sceneCB.pointLightShadowInfo[i] = glm::vec4(-1.0f, 0.22f, 0.002f, 0.9f);

        struct PointCandidate
        {
            EntityID entityId;
            std::string tag;
            glm::vec3 pos;
            float range;
            glm::vec3 color;
            float intensity;
            float distSq;
            bool castShadows;
            float shadowBias;
            float shadowOpacity;
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
                dirShadowOpacity = e.light.shadowOpacity;
                dirNormalBias = e.light.shadowNormalBias;
                dirShadowBlur = e.light.shadowBlur;
                dirShadowMaxDist = e.light.shadowMaxDistance;
            }
            else
            {
                glm::vec3 worldPos = scene.GetWorldPosition(e);
                glm::vec3 diff = worldPos - camera.position;
                float d2 = glm::dot(diff, diff);
                if (e.tag.tag == "Projectile" || e.tag.tag == "EnemyProjectile" || e.tag.tag == "MuzzleFlash")
                    d2 *= 0.05f;
                pointCandidates.push_back({
                    e.id, e.tag.tag, worldPos, e.light.range, e.light.color, e.light.intensity,
                    d2, e.light.castShadows, e.light.shadowBias, e.light.shadowOpacity
                });
            }
        }

        std::sort(pointCandidates.begin(), pointCandidates.end(),
            [](const PointCandidate& a, const PointCandidate& b) { return a.distSq < b.distSq; });

        int pointLightCount = 0;
        struct OmniSlotJob
        {
            int slot = 0;
            PointCandidate light;
        };
        std::vector<OmniSlotJob> omniJobs;
        omniJobs.reserve(8);

        for (const auto& pc : pointCandidates)
        {
            if (pointLightCount >= 8)
                break;

            int assignedSlot = -1;
            if (scene.worldEnv.shadowsEnabled && pc.castShadows && m_ShadowPipeline && m_OmniShadowFramebuffer &&
                static_cast<int>(omniJobs.size()) < 8 &&
                pc.distSq < 48.0f * 48.0f &&
                pc.tag != "Projectile" && pc.tag != "EnemyProjectile" && pc.tag != "MuzzleFlash")
            {
                assignedSlot = static_cast<int>(omniJobs.size());
                omniJobs.push_back({ assignedSlot, pc });
            }

            sceneCB.pointLightPosRange[pointLightCount] = glm::vec4(pc.pos, pc.range);
            sceneCB.pointLightColorIntensity[pointLightCount] = glm::vec4(pc.color, pc.intensity);
            sceneCB.pointLightShadowInfo[pointLightCount] = glm::vec4(
                static_cast<float>(assignedSlot), 0.22f, std::max(pc.shadowBias, 0.0012f), pc.shadowOpacity);
            ++pointLightCount;
        }

        // 0a. 360-degree 6-Face OmniPointShadowAtlas Pass (up to 4 nearest shadow-casting point lights)
        if (!omniJobs.empty())
        {
            nvrhi::utils::ClearDepthStencilAttachment(cmd, m_OmniShadowFramebuffer, 1.0f, 0);

            static const glm::vec3 kFaceDirs[6] = {
                glm::vec3( 1.0f,  0.0f,  0.0f),
                glm::vec3(-1.0f,  0.0f,  0.0f),
                glm::vec3( 0.0f,  1.0f,  0.0f),
                glm::vec3( 0.0f, -1.0f,  0.0f),
                glm::vec3( 0.0f,  0.0f,  1.0f),
                glm::vec3( 0.0f,  0.0f, -1.0f)
            };
            static const glm::vec3 kFaceUps[6] = {
                glm::vec3(0.0f, 1.0f,  0.0f),
                glm::vec3(0.0f, 1.0f,  0.0f),
                glm::vec3(0.0f, 0.0f, -1.0f),
                glm::vec3(0.0f, 0.0f,  1.0f),
                glm::vec3(0.0f, 1.0f,  0.0f),
                glm::vec3(0.0f, 1.0f,  0.0f)
            };

            for (const auto& job : omniJobs)
            {
                const float lightRange = std::max(job.light.range, 0.5f);
                const glm::mat4 faceProj = glm::perspectiveLH_ZO(glm::radians(90.0f), 1.0f, 0.22f, lightRange);
                const int slotX = job.slot & 3;
                const int slotY = job.slot >> 2;

                for (int face = 0; face < 6; ++face)
                {
                    const glm::mat4 faceView = glm::lookAtLH(job.light.pos, job.light.pos + kFaceDirs[face], kFaceUps[face]);
                    const glm::mat4 faceVP = faceProj * faceView;

                    ShadowCBData shadowCB{ faceVP };
                    cmd->writeBuffer(m_ShadowCB, &shadowCB, sizeof(shadowCB));

                    const int col = face % 3;
                    const int row = face / 3;
                    const float tileX = static_cast<float>(slotX * 768 + col * 256);
                    const float tileY = static_cast<float>(slotY * 512 + row * 256);
                    nvrhi::ViewportState tileVp = nvrhi::ViewportState()
                        .addViewportAndScissorRect(nvrhi::Viewport(tileX, tileX + 256.0f, tileY, tileY + 256.0f, 0.0f, 1.0f));

                    FrustumCuller faceFrustum;
                    faceFrustum.ExtractFromMatrix(faceVP);

                    for (const auto& e : scene.GetEntities())
                    {
                        if (!e.alive || !e.mesh.enabled || !e.mesh.castShadows) continue;
                        if (e.id == job.light.entityId || e.parentId == job.light.entityId) continue;
                        if (e.tag.tag == "ViewModel" || e.tag.tag == "MuzzleFlash" ||
                            e.tag.tag == "Projectile" || e.tag.tag == "EnemyProjectile")
                            continue;

                        glm::mat4 worldMat = scene.GetWorldMatrix(e);
                        if (e.tag.tag != "LevelGeometry")
                        {
                            glm::vec3 wp = glm::vec3(worldMat[3]);
                            glm::vec3 halfExt = glm::max(glm::abs(e.transform.scale) * 1.35f, glm::vec3(1.2f));
                            float boundR = glm::length(halfExt);
                            if (glm::length(wp - job.light.pos) - boundR > lightRange)
                                continue;
                            if (!faceFrustum.InFrustum(wp - halfExt, wp + halfExt))
                                continue;
                        }

                        const GPUMesh& gpuMesh = GetOrCreateMesh(cmd, e.mesh);
                        if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0)
                            continue;

                        ObjectCBData objCB{};
                        objCB.model = worldMat;
                        cmd->writeBuffer(m_ObjectCB, &objCB, sizeof(objCB));

                        nvrhi::GraphicsState state;
                        state.pipeline = m_ShadowPipeline;
                        state.framebuffer = m_OmniShadowFramebuffer;
                        state.viewport = tileVp;
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
            }
        }

        // 0b. 2-Cascade Directional Shadow Map (PSSM 2048x1024: Cascade 0 Near Crisp + Cascade 1 Far Arena)
        bool shadowsOn = scene.worldEnv.shadowsEnabled && dirCastShadows && m_ShadowPipeline && m_ShadowFramebuffer;
        glm::mat4 lightViewProj0(1.0f);
        glm::mat4 lightViewProj1(1.0f);
        if (shadowsOn)
        {
            nvrhi::utils::ClearDepthStencilAttachment(cmd, m_ShadowFramebuffer, 1.0f, 0);

            glm::vec3 flatFwd(camera.forward.x, 0.0f, camera.forward.z);
            float ffLen = glm::length(flatFwd);
            glm::vec3 dirFwd = (ffLen > 0.05f) ? (flatFwd / ffLen) : glm::vec3(0.0f, 0.0f, 1.0f);
            glm::vec3 lightUp = (std::abs(sunDir.y) > 0.98f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

            const float cascadeHalf[2] = {
                18.0f,
                std::clamp(std::max(scene.arenaHalfSize * 1.15f, dirShadowMaxDist * 0.65f), 42.0f, 68.0f)
            };
            const float cascadeLead[2] = { 10.0f, 24.0f };
            const float cascadePullback[2] = { 70.0f, 85.0f };
            const float cascadeZFar[2] = { 175.0f, 210.0f };

            for (int c = 0; c < 2; ++c)
            {
                glm::vec3 focus = camera.position + dirFwd * cascadeLead[c];
                focus.y = camera.position.y * (c == 0 ? 0.45f : 0.35f);

                glm::mat4 tmpView = glm::lookAtLH(focus + sunDir * cascadePullback[c], focus, lightUp);
                float texelWorld = (2.0f * cascadeHalf[c]) / 1024.0f;
                glm::vec3 rightAxis(tmpView[0][0], tmpView[1][0], tmpView[2][0]);
                glm::vec3 upAxis(tmpView[0][1], tmpView[1][1], tmpView[2][1]);
                float sx = std::floor(glm::dot(focus, rightAxis) / texelWorld + 0.5f) * texelWorld;
                float sy = std::floor(glm::dot(focus, upAxis) / texelWorld + 0.5f) * texelWorld;
                glm::vec3 snappedFocus = rightAxis * sx + upAxis * sy + sunDir * glm::dot(focus, sunDir);

                glm::mat4 lightView = glm::lookAtLH(snappedFocus + sunDir * cascadePullback[c], snappedFocus, lightUp);
                glm::mat4 lightProj = glm::orthoLH_ZO(-cascadeHalf[c], cascadeHalf[c], -cascadeHalf[c], cascadeHalf[c], 1.0f, cascadeZFar[c]);
                glm::mat4 cascadeVP = lightProj * lightView;
                if (c == 0) lightViewProj0 = cascadeVP;
                else        lightViewProj1 = cascadeVP;

                ShadowCBData shadowCB{ cascadeVP };
                cmd->writeBuffer(m_ShadowCB, &shadowCB, sizeof(shadowCB));

                nvrhi::ViewportState shadowVp = nvrhi::ViewportState()
                    .addViewportAndScissorRect(nvrhi::Viewport(c * 1024.0f, (c + 1) * 1024.0f, 0.0f, 1024.0f, 0.0f, 1.0f));

                FrustumCuller cascadeFrustum;
                cascadeFrustum.ExtractFromMatrix(cascadeVP);

                for (const auto& e : scene.GetEntities())
                {
                    if (!e.alive || !e.mesh.enabled || !e.mesh.castShadows) continue;
                    if (e.tag.tag == "ViewModel" || e.tag.tag == "MuzzleFlash" ||
                        e.tag.tag == "Projectile" || e.tag.tag == "EnemyProjectile")
                        continue;

                    glm::mat4 worldMat = scene.GetWorldMatrix(e);
                    if (e.tag.tag != "LevelGeometry")
                    {
                        glm::vec3 wp = glm::vec3(worldMat[3]);
                        glm::vec3 halfExt = glm::max(glm::abs(e.transform.scale) * 1.35f, glm::vec3(1.2f));
                        if (!cascadeFrustum.InFrustum(wp - halfExt, wp + halfExt))
                            continue;
                    }

                    const GPUMesh& gpuMesh = GetOrCreateMesh(cmd, e.mesh);
                    if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0)
                        continue;

                    ObjectCBData objCB{};
                    objCB.model = worldMat;
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
        }

        // 0c. Camera Depth Prepass + Half-Res Bilateral SSAO (Godot Environment::ssao_enabled)
        bool ssaoActive = scene.worldEnv.ssaoEnabled && m_DepthPrepassPipeline && m_SSAOPipeline && m_SSAOBlurPipeline && m_PrepassFramebuffer;
        if (ssaoActive)
        {
            nvrhi::utils::ClearDepthStencilAttachment(cmd, m_PrepassFramebuffer, 1.0f, 0);
            ShadowCBData camCB{ sceneCB.viewProj };
            cmd->writeBuffer(m_ShadowCB, &camCB, sizeof(camCB));

            FrustumCuller camFrustum;
            camFrustum.ExtractFromMatrix(sceneCB.viewProj);

            for (const auto& e : scene.GetEntities())
            {
                if (!e.alive || !e.mesh.enabled) continue;
                if (e.tag.tag == "ViewModel" || e.tag.tag == "MuzzleFlash") continue;

                glm::mat4 worldMat = scene.GetWorldMatrix(e);
                if (scene.worldEnv.frustumCullingEnabled && e.tag.tag != "LevelGeometry")
                {
                    glm::vec3 wp = glm::vec3(worldMat[3]);
                    glm::vec3 halfExt = glm::max(glm::abs(e.transform.scale) * 1.35f, glm::vec3(1.2f));
                    if (!camFrustum.InFrustum(wp - halfExt, wp + halfExt))
                        continue;
                }

                const GPUMesh& gpuMesh = GetOrCreateMesh(cmd, e.mesh);
                if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0)
                    continue;

                ObjectCBData objCB{};
                objCB.model = worldMat;
                cmd->writeBuffer(m_ObjectCB, &objCB, sizeof(objCB));

                nvrhi::GraphicsState state;
                state.pipeline = m_DepthPrepassPipeline;
                state.framebuffer = m_PrepassFramebuffer;
                state.viewport = viewportState;
                state.bindings = { m_ShadowBindingSet };
                state.vertexBuffers = { { gpuMesh.vertexBuffer, 0, 0 } };
                state.indexBuffer = { gpuMesh.indexBuffer, nvrhi::Format::R32_UINT, 0 };
                cmd->setGraphicsState(state);

                nvrhi::DrawArguments args;
                args.vertexCount = gpuMesh.indexCount;
                cmd->drawIndexed(args);
                ++m_Stats.drawCalls;
            }

            uint32_t halfW = std::max(vpW / 2u, 32u);
            uint32_t halfH = std::max(vpH / 2u, 32u);
            nvrhi::ViewportState halfVp = nvrhi::ViewportState()
                .addViewportAndScissorRect(nvrhi::Viewport(static_cast<float>(halfW), static_cast<float>(halfH)));

            SSAOCBData ssaoCB{};
            ssaoCB.proj = camera.GetProjectionMatrix();
            ssaoCB.invProj = glm::inverse(ssaoCB.proj);
            ssaoCB.ssaoParams = glm::vec4(scene.worldEnv.ssaoRadius, scene.worldEnv.ssaoIntensity, 0.035f, 1.0f);
            ssaoCB.viewportInfo = glm::vec4(1.0f / static_cast<float>(vpW), 1.0f / static_cast<float>(vpH), timeSeconds, 0.0f);
            cmd->writeBuffer(m_SSAOCB, &ssaoCB, sizeof(ssaoCB));

            {
                nvrhi::GraphicsState state;
                state.pipeline = m_SSAOPipeline;
                state.framebuffer = m_SSAOFramebuffer;
                state.viewport = halfVp;
                state.bindings = { m_SSAOBindingSet };
                cmd->setGraphicsState(state);

                nvrhi::DrawArguments args;
                args.vertexCount = 3;
                cmd->draw(args);
                ++m_Stats.drawCalls;
            }
            {
                nvrhi::GraphicsState state;
                state.pipeline = m_SSAOBlurPipeline;
                state.framebuffer = m_SSAOBlurFramebuffer;
                state.viewport = halfVp;
                state.bindings = { m_SSAOBlurBindingSet };
                cmd->setGraphicsState(state);

                nvrhi::DrawArguments args;
                args.vertexCount = 3;
                cmd->draw(args);
                ++m_Stats.drawCalls;
            }
        }

        // Clear main target color & depth for Skybox + PBR Pass
        nvrhi::utils::ClearColorAttachment(cmd, targetFramebuffer, 0, nvrhi::Color(0.04f, 0.06f, 0.12f, 1.0f));
        nvrhi::utils::ClearDepthStencilAttachment(cmd, targetFramebuffer, 1.0f, 0);

        sceneCB.lightViewProj0 = lightViewProj0;
        sceneCB.lightViewProj1 = lightViewProj1;
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
            dirShadowOpacity);
        sceneCB.shadowParams = glm::vec4(
            dirShadowBias,
            dirNormalBias,
            dirShadowBlur,
            shadowsOn ? 1.0f : 0.0f);
        sceneCB.ssaoParams = glm::vec4(
            ssaoActive ? 1.0f : 0.0f,
            scene.worldEnv.ssaoDirectAffect,
            1.0f / static_cast<float>(vpW),
            1.0f / static_cast<float>(vpH));
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

        // Godot RayCast3D Debug Sensor Lines (in Editor / Collider View)
        if (drawEditorGrid || drawColliders)
        {
            for (const auto& e : scene.GetEntities())
            {
                if (!e.alive || !e.raycast.enabled || !e.raycast.showDebugRay) continue;
                glm::vec3 start = scene.GetWorldPosition(e);
                glm::vec3 localEnd = e.raycast.targetPosition;
                glm::vec3 worldEnd = glm::vec3(scene.GetWorldMatrix(e) * glm::vec4(localEnd, 1.0f));
                glm::vec3 endPt = e.raycast.isColliding ? e.raycast.hitPoint : worldEnd;
                glm::vec4 beamCol = e.raycast.isColliding
                    ? glm::vec4(1.0f, 0.24f, 0.16f, 1.0f)
                    : glm::vec4(0.18f, 0.96f, 0.65f, 1.0f);
                AddLine(worldLines, start, endPt, beamCol);
                if (e.raycast.isColliding)
                {
                    AddLine(worldLines, e.raycast.hitPoint, e.raycast.hitPoint + e.raycast.hitNormal * 0.65f, glm::vec4(1.0f, 0.88f, 0.20f, 1.0f));
                }
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
