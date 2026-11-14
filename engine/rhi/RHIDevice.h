#pragma once
#include <nvrhi/nvrhi.h>
#include <string>
#include <vector>
#include <cstdint>

namespace Hargon
{
    class Window;

    enum class GraphicsBackend
    {
        Vulkan,
        D3D11
    };

    struct OffscreenTarget
    {
        uint32_t width = 0;
        uint32_t height = 0;
        nvrhi::TextureHandle colorTexture;
        nvrhi::TextureHandle depthTexture;
        nvrhi::FramebufferHandle framebuffer;
    };

    class RHIDevice
    {
    public:
        RHIDevice() = default;
        ~RHIDevice();

        bool Init(Window* window, GraphicsBackend preferredBackend = GraphicsBackend::Vulkan, bool enableValidation = false);
        void Shutdown();

        void Resize(uint32_t width, uint32_t height);

        bool BeginFrame();
        void FinishFrameAndPresent(nvrhi::ICommandList* commandList);
        void WaitForIdle();

        nvrhi::IDevice* GetDevice() const { return m_NvrhiDevice; }
        nvrhi::IFramebuffer* GetCurrentFramebuffer() const;
        nvrhi::ITexture* GetCurrentBackbuffer() const;
        nvrhi::Format GetBackbufferFormat() const { return m_BackbufferFormat; }
        nvrhi::Format GetDepthFormat() const { return m_DepthFormat; }

        GraphicsBackend GetBackend() const { return m_ActiveBackend; }
        std::string GetBackendName() const;
        std::string GetGPUName() const { return m_GPUName; }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        // Shader creation helper (uses SPIR-V for Vulkan, HLSL source/file for D3D11)
        nvrhi::ShaderHandle CreateShader(
            nvrhi::ShaderType stage,
            const std::string& debugName,
            const std::string& entryPoint,
            const void* spirvData,
            size_t spirvByteSize,
            const std::string& hlslFilePath,
            const char* embeddedHlslFallback = nullptr);

        // Offscreen render target helper for Editor Viewport
        OffscreenTarget CreateOffscreenTarget(uint32_t width, uint32_t height);

        // Readback a texture to a BMP image file for screenshots / visual verification
        bool SaveTextureToBMP(nvrhi::ITexture* texture, const std::string& filePath);

    private:
        bool InitVulkan(Window* window, bool enableValidation);
        void CreateVulkanSwapchain(uint32_t width, uint32_t height);
        void DestroyVulkanSwapchain();
        void ShutdownVulkan();

#ifdef _WIN32
        bool InitD3D11(Window* window, bool enableValidation);
        void CreateD3D11SwapchainResources(uint32_t width, uint32_t height);
        void DestroyD3D11SwapchainResources();
        void ShutdownD3D11();
#endif

        void CreateDepthAndFramebuffers();

    private:
        Window* m_Window = nullptr;
        GraphicsBackend m_ActiveBackend = GraphicsBackend::Vulkan;
        std::string m_GPUName = "Unknown GPU";
        uint32_t m_Width = 1600;
        uint32_t m_Height = 900;

        nvrhi::Format m_BackbufferFormat = nvrhi::Format::BGRA8_UNORM;
        nvrhi::Format m_DepthFormat = nvrhi::Format::D32;

        nvrhi::DeviceHandle m_RawDevice;
        nvrhi::DeviceHandle m_NvrhiDevice;

        std::vector<nvrhi::TextureHandle> m_SwapchainTextures;
        nvrhi::TextureHandle m_DepthTexture;
        std::vector<nvrhi::FramebufferHandle> m_SwapchainFramebuffers;
        uint32_t m_CurrentBackbufferIndex = 0;
        uint32_t m_FrameIndex = 0;

        struct Impl;
        Impl* m_Impl = nullptr;
    };
}
