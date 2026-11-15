#pragma once
#include "Mesh.h"
#include "../rhi/RHIDevice.h"
#include "../scene/Scene.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <string>

namespace Hargon
{
    enum class GizmoMode
    {
        Translate = 0,
        Rotate = 1,
        Scale = 2
    };

    struct CameraView
    {
        glm::vec3 position{ 0.0f, 12.0f, -18.0f };
        glm::vec3 forward{ 0.0f, -0.45f, 0.89f };
        glm::vec3 up{ 0.0f, 1.0f, 0.0f };
        float fovDegrees = 55.0f;
        float nearClip = 0.1f;
        float farClip = 350.0f;
        float aspectRatio = 16.0f / 9.0f;

        glm::mat4 GetViewMatrix() const;
        glm::mat4 GetProjectionMatrix() const;
        glm::mat4 GetViewProjectionMatrix() const;
    };

    struct LineVertex
    {
        glm::vec3 position;
        glm::vec4 color;
    };

    struct Particle3D
    {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec4 color;
        float life = 0.0f;
        float maxLife = 0.6f;
        float size = 0.35f;
    };

    struct RenderStats
    {
        uint32_t drawCalls = 0;
        uint32_t triangleCount = 0;
        uint32_t entityCount = 0;
        uint32_t culledEntities = 0;
        uint32_t activePointLights = 0;
        uint32_t activeParticles = 0;
    };

    class Renderer3D
    {
    public:
        Renderer3D() = default;
        ~Renderer3D();

        bool Init(RHIDevice* rhi);
        void Shutdown();

        void UpdateParticles(float dt);
        void SpawnParticleBurst(const glm::vec3& position, const glm::vec3& color, int count = 16, float speed = 8.0f);
        void AddTransientLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
        void RegisterCustomMesh(const std::string& key, const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices);
        void ClearCustomMeshes();

        void RenderScene(
            nvrhi::ICommandList* cmd,
            nvrhi::IFramebuffer* targetFramebuffer,
            const Scene& scene,
            const CameraView& camera,
            float timeSeconds,
            bool drawEditorGrid = false,
            EntityID selectedEntity = kInvalidEntity,
            GizmoMode gizmoMode = GizmoMode::Translate,
            int hoveredGizmoAxis = -1,
            bool drawColliders = false,
            bool localGizmoSpace = false,
            const std::vector<EntityID>* additionalSelected = nullptr);

        // Ray-test against the 3D Transform Gizmo axes (returns 0=X, 1=Y, 2=Z, or -1=None)
        int RaycastGizmo(
            const glm::vec3& gizmoCenter,
            const CameraView& camera,
            const glm::vec3& rayOrigin,
            const glm::vec3& rayDir,
            float* outAxisScale = nullptr,
            const glm::mat3& gizmoBasis = glm::mat3(1.0f)) const;

        bool TryGetMeshLocalBounds(const MeshComponent& meshComp, glm::vec3& outMin, glm::vec3& outMax) const;

        const RenderStats& GetStats() const { return m_Stats; }

    private:
        void EnsurePipelines(nvrhi::IFramebuffer* targetFramebuffer);
        void EnsurePrepassResources(uint32_t vpW, uint32_t vpH);
        const GPUMesh& GetOrCreateMesh(nvrhi::ICommandList* cmd, const MeshComponent& meshComp);

        void AddLine(std::vector<LineVertex>& lines, const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
        void AddWireBox(std::vector<LineVertex>& lines, const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color);
        void AddWireSphere(std::vector<LineVertex>& lines, const glm::vec3& center, float radius, const glm::vec4& color);
        void BuildGizmoLines(std::vector<LineVertex>& lines, const glm::vec3& center, float scale, GizmoMode mode, int hoveredAxis, const glm::mat3& basis = glm::mat3(1.0f));

    private:
        RHIDevice* m_RHI = nullptr;
        nvrhi::IDevice* m_Device = nullptr;

        // Shaders
        nvrhi::ShaderHandle m_PBRVS;
        nvrhi::ShaderHandle m_PBRPS;
        nvrhi::ShaderHandle m_ShadowVS;
        nvrhi::ShaderHandle m_SSAOVS;
        nvrhi::ShaderHandle m_SSAOPS;
        nvrhi::ShaderHandle m_SSAOBlurPS;
        nvrhi::ShaderHandle m_SkyboxVS;
        nvrhi::ShaderHandle m_SkyboxPS;
        nvrhi::ShaderHandle m_LineVS;
        nvrhi::ShaderHandle m_LinePS;

        // Input layouts
        nvrhi::InputLayoutHandle m_MeshInputLayout;
        nvrhi::InputLayoutHandle m_LineInputLayout;

        // Constant Buffers
        nvrhi::BufferHandle m_SceneCB;
        nvrhi::BufferHandle m_ObjectCB;
        nvrhi::BufferHandle m_ShadowCB;
        nvrhi::BufferHandle m_SSAOCB;
        nvrhi::BufferHandle m_SkyboxCB;
        nvrhi::BufferHandle m_LineCB;

        // Dynamic vertex buffers for lines and gizmos
        nvrhi::BufferHandle m_DynamicLineVB;
        nvrhi::BufferHandle m_DynamicGizmoVB;
        uint32_t m_MaxLineVertices = 16384;

        // Default white texture + 2-cascade PSSM + 6-face OmniShadowAtlas + Depth Prepass / SSAO + samplers
        nvrhi::TextureHandle m_WhiteTexture;
        nvrhi::TextureHandle m_ShadowMapTexture;
        nvrhi::FramebufferHandle m_ShadowFramebuffer;
        nvrhi::TextureHandle m_OmniShadowAtlas;
        nvrhi::FramebufferHandle m_OmniShadowFramebuffer;
        nvrhi::TextureHandle m_PrepassDepthTexture;
        nvrhi::FramebufferHandle m_PrepassFramebuffer;
        nvrhi::TextureHandle m_SSAOTexture;
        nvrhi::FramebufferHandle m_SSAOFramebuffer;
        nvrhi::TextureHandle m_SSAOBlurTexture;
        nvrhi::FramebufferHandle m_SSAOBlurFramebuffer;
        uint32_t m_PrepassWidth = 0;
        uint32_t m_PrepassHeight = 0;
        nvrhi::SamplerHandle m_LinearSampler;
        nvrhi::SamplerHandle m_ShadowClampSampler;
        nvrhi::SamplerHandle m_PointClampSampler;

        // Binding layouts and sets
        nvrhi::BindingLayoutHandle m_PBRBindingLayout;
        nvrhi::BindingSetHandle m_PBRBindingSet;
        nvrhi::BindingLayoutHandle m_ShadowBindingLayout;
        nvrhi::BindingSetHandle m_ShadowBindingSet;
        nvrhi::BindingLayoutHandle m_SSAOBindingLayout;
        nvrhi::BindingSetHandle m_SSAOBindingSet;
        nvrhi::BindingLayoutHandle m_SSAOBlurBindingLayout;
        nvrhi::BindingSetHandle m_SSAOBlurBindingSet;
        nvrhi::BindingLayoutHandle m_SkyboxBindingLayout;
        nvrhi::BindingSetHandle m_SkyboxBindingSet;
        nvrhi::BindingLayoutHandle m_LineBindingLayout;
        nvrhi::BindingSetHandle m_LineBindingSet;

        // Graphics pipelines (rebuilt automatically if target FramebufferInfo changes)
        nvrhi::FramebufferInfo m_CachedFbInfo;
        bool m_HasCachedFbInfo = false;
        nvrhi::GraphicsPipelineHandle m_ShadowPipeline;
        nvrhi::GraphicsPipelineHandle m_DepthPrepassPipeline;
        nvrhi::GraphicsPipelineHandle m_SSAOPipeline;
        nvrhi::GraphicsPipelineHandle m_SSAOBlurPipeline;
        nvrhi::GraphicsPipelineHandle m_PBRPipeline;
        nvrhi::GraphicsPipelineHandle m_SkyboxPipeline;
        nvrhi::GraphicsPipelineHandle m_LinePipeline;
        nvrhi::GraphicsPipelineHandle m_GizmoPipeline;

        // Mesh cache
        std::unordered_map<int, GPUMesh> m_PrimitiveMeshes;
        std::unordered_map<std::string, GPUMesh> m_CustomMeshes;

        // Particles & Transient 3D Lines (Lightning beams, tracers)
        std::vector<Particle3D> m_Particles;
        std::vector<LineVertex> m_TransientLines;
        uint32_t m_RngState = 0x13579BDFu;

        RenderStats m_Stats{};
    };
}
