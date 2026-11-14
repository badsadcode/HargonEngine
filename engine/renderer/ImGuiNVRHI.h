#pragma once
#include "../rhi/RHIDevice.h"
#include <nvrhi/nvrhi.h>
#include <unordered_map>

namespace Hargon
{
    class Window;

    class ImGuiNVRHI
    {
    public:
        ImGuiNVRHI() = default;
        ~ImGuiNVRHI();

        bool Init(RHIDevice* rhi, Window* window);
        void Shutdown();

        void BeginFrame();
        void Render(nvrhi::ICommandList* cmd, nvrhi::IFramebuffer* targetFramebuffer);

        // Clears cached texture binding sets when an offscreen texture is resized
        void InvalidateTextureCache();

    private:
        void EnsurePipeline(nvrhi::IFramebuffer* targetFramebuffer);
        nvrhi::IBindingSet* GetOrCreateBindingSet(nvrhi::ITexture* texture);

    private:
        RHIDevice* m_RHI = nullptr;
        nvrhi::IDevice* m_Device = nullptr;
        bool m_Initialized = false;

        nvrhi::ShaderHandle m_VS;
        nvrhi::ShaderHandle m_PS;
        nvrhi::InputLayoutHandle m_InputLayout;
        nvrhi::BufferHandle m_ConstantBuffer;
        nvrhi::TextureHandle m_FontTexture;
        nvrhi::SamplerHandle m_Sampler;
        nvrhi::BindingLayoutHandle m_BindingLayout;

        nvrhi::BufferHandle m_VertexBuffer;
        nvrhi::BufferHandle m_IndexBuffer;
        uint32_t m_MaxVertices = 65536;
        uint32_t m_MaxIndices = 131072;

        nvrhi::FramebufferInfo m_CachedFbInfo;
        bool m_HasCachedFbInfo = false;
        nvrhi::GraphicsPipelineHandle m_Pipeline;

        std::unordered_map<nvrhi::ITexture*, nvrhi::BindingSetHandle> m_BindingSets;
    };
}
