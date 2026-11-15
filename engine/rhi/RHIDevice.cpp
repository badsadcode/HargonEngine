#include "RHIDevice.h"
#include "../core/Window.h"
#include "../core/Log.h"

#include <nvrhi/validation.h>
#include <nvrhi/utils.h>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <nvrhi/vulkan.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <nvrhi/d3d11.h>
#endif

#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace Hargon
{
    class NvrhiMessageCallback : public nvrhi::IMessageCallback
    {
    public:
        void message(nvrhi::MessageSeverity severity, const char* messageText) override
        {
            if (severity == nvrhi::MessageSeverity::Error || severity == nvrhi::MessageSeverity::Fatal)
                Log::Error(std::string("[NVRHI] ") + messageText);
            else
                Log::RHI(std::string("[NVRHI] ") + messageText);
        }
    };

    static NvrhiMessageCallback s_MessageCallback;

    struct RHIDevice::Impl
    {
        // Vulkan state
        VkInstance vkInstance = VK_NULL_HANDLE;
        VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
        VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
        VkDevice vkDevice = VK_NULL_HANDLE;
        VkQueue vkGraphicsQueue = VK_NULL_HANDLE;
        uint32_t vkQueueFamily = 0;
        VkSwapchainKHR vkSwapchain = VK_NULL_HANDLE;
        std::vector<VkImage> vkSwapchainImages;
        std::vector<VkSemaphore> acquireSemaphores;
        std::vector<VkSemaphore> presentSemaphores;
        size_t semaphoreIndex = 0;
        nvrhi::vulkan::DeviceHandle vkNvrhiDevice;

#ifdef _WIN32
        // D3D11 state
        Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain> dxgiSwapChain;
#endif
    };

    RHIDevice::~RHIDevice()
    {
        Shutdown();
    }

    bool RHIDevice::Init(Window* window, GraphicsBackend preferredBackend, bool enableValidation)
    {
        m_Window = window;
        m_Width = std::max(1u, window->GetWidth());
        m_Height = std::max(1u, window->GetHeight());
        m_Impl = new Impl();

        if (preferredBackend == GraphicsBackend::Vulkan)
        {
            if (InitVulkan(window, enableValidation))
            {
                m_ActiveBackend = GraphicsBackend::Vulkan;
                Log::RHI("Initialized NVRHI with Vulkan backend on GPU: " + m_GPUName);
                return true;
            }
            Log::RHI("Vulkan initialization failed, attempting D3D11 fallback...");
        }

#ifdef _WIN32
        if (InitD3D11(window, enableValidation))
        {
            m_ActiveBackend = GraphicsBackend::D3D11;
            Log::RHI("Initialized NVRHI with D3D11 backend on GPU: " + m_GPUName);
            return true;
        }
#endif

        Log::Error("Failed to initialize any NVRHI graphics backend!");
        return false;
    }

    void RHIDevice::Shutdown()
    {
        if (!m_Impl)
            return;

        WaitForIdle();

        m_SwapchainFramebuffers.clear();
        m_DepthTexture = nullptr;
        m_SwapchainTextures.clear();
        m_NvrhiDevice = nullptr;
        m_RawDevice = nullptr;

        if (m_ActiveBackend == GraphicsBackend::Vulkan)
            ShutdownVulkan();
#ifdef _WIN32
        else if (m_ActiveBackend == GraphicsBackend::D3D11)
            ShutdownD3D11();
#endif

        delete m_Impl;
        m_Impl = nullptr;
    }

    bool RHIDevice::InitVulkan(Window* window, bool enableValidation)
    {
        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        if (!glfwExts || glfwExtCount == 0)
        {
            Log::Error("GLFW failed to find required Vulkan instance extensions.");
            return false;
        }

        std::vector<const char*> instanceExtensions(glfwExts, glfwExts + glfwExtCount);

        VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.pApplicationName = "Hargon Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Hargon";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo instInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        instInfo.pApplicationInfo = &appInfo;
        instInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        instInfo.ppEnabledExtensionNames = instanceExtensions.data();

        if (vkCreateInstance(&instInfo, nullptr, &m_Impl->vkInstance) != VK_SUCCESS)
        {
            Log::Error("Failed to create Vulkan instance.");
            return false;
        }

        if (glfwCreateWindowSurface(m_Impl->vkInstance, window->GetNativeWindow(), nullptr, &m_Impl->vkSurface) != VK_SUCCESS)
        {
            Log::Error("Failed to create GLFW Vulkan window surface.");
            return false;
        }

        uint32_t gpuCount = 0;
        vkEnumeratePhysicalDevices(m_Impl->vkInstance, &gpuCount, nullptr);
        if (gpuCount == 0)
        {
            Log::Error("No Vulkan physical devices found.");
            return false;
        }

        std::vector<VkPhysicalDevice> gpus(gpuCount);
        vkEnumeratePhysicalDevices(m_Impl->vkInstance, &gpuCount, gpus.data());

        // Prefer Discrete GPU
        m_Impl->vkPhysicalDevice = gpus[0];
        for (VkPhysicalDevice gpu : gpus)
        {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(gpu, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                m_Impl->vkPhysicalDevice = gpu;
                break;
            }
        }

        VkPhysicalDeviceProperties chosenProps{};
        vkGetPhysicalDeviceProperties(m_Impl->vkPhysicalDevice, &chosenProps);
        m_GPUName = chosenProps.deviceName;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_Impl->vkPhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_Impl->vkPhysicalDevice, &queueFamilyCount, queueFamilies.data());

        bool foundQueue = false;
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_Impl->vkPhysicalDevice, i, m_Impl->vkSurface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport)
            {
                m_Impl->vkQueueFamily = i;
                foundQueue = true;
                break;
            }
        }

        if (!foundQueue)
        {
            Log::Error("Failed to find Vulkan queue family supporting both graphics and present.");
            return false;
        }

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueInfo.queueFamilyIndex = m_Impl->vkQueueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkPhysicalDeviceVulkan12Features vk12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        vk12Features.timelineSemaphore = VK_TRUE;

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.fillModeNonSolid = VK_TRUE;

        VkDeviceCreateInfo devInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        devInfo.pNext = &vk12Features;
        devInfo.queueCreateInfoCount = 1;
        devInfo.pQueueCreateInfos = &queueInfo;
        devInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        devInfo.ppEnabledExtensionNames = deviceExtensions.data();
        devInfo.pEnabledFeatures = &deviceFeatures;

        if (vkCreateDevice(m_Impl->vkPhysicalDevice, &devInfo, nullptr, &m_Impl->vkDevice) != VK_SUCCESS)
        {
            Log::Error("Failed to create Vulkan logical device.");
            return false;
        }

        vkGetDeviceQueue(m_Impl->vkDevice, m_Impl->vkQueueFamily, 0, &m_Impl->vkGraphicsQueue);

        // Initialize dynamic Vulkan-Hpp dispatcher used by NVRHI
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance(m_Impl->vkInstance), vk::Device(m_Impl->vkDevice));

        nvrhi::vulkan::DeviceDesc nvrhiDesc{};
        nvrhiDesc.errorCB = &s_MessageCallback;
        nvrhiDesc.instance = m_Impl->vkInstance;
        nvrhiDesc.physicalDevice = m_Impl->vkPhysicalDevice;
        nvrhiDesc.device = m_Impl->vkDevice;
        nvrhiDesc.graphicsQueue = m_Impl->vkGraphicsQueue;
        nvrhiDesc.graphicsQueueIndex = static_cast<int>(m_Impl->vkQueueFamily);
        nvrhiDesc.instanceExtensions = instanceExtensions.data();
        nvrhiDesc.numInstanceExtensions = instanceExtensions.size();
        nvrhiDesc.deviceExtensions = deviceExtensions.data();
        nvrhiDesc.numDeviceExtensions = deviceExtensions.size();

        m_Impl->vkNvrhiDevice = nvrhi::vulkan::createDevice(nvrhiDesc);
        if (!m_Impl->vkNvrhiDevice)
        {
            Log::Error("nvrhi::vulkan::createDevice failed.");
            return false;
        }

        m_RawDevice = m_Impl->vkNvrhiDevice;
        if (enableValidation)
            m_NvrhiDevice = nvrhi::validation::createValidationLayer(m_RawDevice);
        else
            m_NvrhiDevice = m_RawDevice;

        CreateVulkanSwapchain(m_Width, m_Height);
        CreateDepthAndFramebuffers();
        return true;
    }

    void RHIDevice::CreateVulkanSwapchain(uint32_t width, uint32_t height)
    {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Impl->vkPhysicalDevice, m_Impl->vkSurface, &caps);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_Impl->vkPhysicalDevice, m_Impl->vkSurface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_Impl->vkPhysicalDevice, m_Impl->vkSurface, &formatCount, formats.data());

        VkSurfaceFormatKHR chosenFormat = formats[0];
        m_BackbufferFormat = nvrhi::Format::BGRA8_UNORM;

        for (const auto& fmt : formats)
        {
            if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                chosenFormat = fmt;
                m_BackbufferFormat = nvrhi::Format::BGRA8_UNORM;
                break;
            }
            if (fmt.format == VK_FORMAT_R8G8B8A8_UNORM)
            {
                chosenFormat = fmt;
                m_BackbufferFormat = nvrhi::Format::RGBA8_UNORM;
                break;
            }
        }

        VkExtent2D extent{ width, height };
        if (caps.currentExtent.width != UINT32_MAX)
        {
            extent = caps.currentExtent;
        }
        else
        {
            extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }

        m_Width = std::max(1u, extent.width);
        m_Height = std::max(1u, extent.height);

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
            imageCount = caps.maxImageCount;

        VkSwapchainCreateInfoKHR swapInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        swapInfo.surface = m_Impl->vkSurface;
        swapInfo.minImageCount = imageCount;
        swapInfo.imageFormat = chosenFormat.format;
        swapInfo.imageColorSpace = chosenFormat.colorSpace;
        swapInfo.imageExtent = { m_Width, m_Height };
        swapInfo.imageArrayLayers = 1;
        swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapInfo.preTransform = caps.currentTransform;
        swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapInfo.clipped = VK_TRUE;
        swapInfo.oldSwapchain = VK_NULL_HANDLE;

        vkCreateSwapchainKHR(m_Impl->vkDevice, &swapInfo, nullptr, &m_Impl->vkSwapchain);

        uint32_t actualImages = 0;
        vkGetSwapchainImagesKHR(m_Impl->vkDevice, m_Impl->vkSwapchain, &actualImages, nullptr);
        m_Impl->vkSwapchainImages.resize(actualImages);
        vkGetSwapchainImagesKHR(m_Impl->vkDevice, m_Impl->vkSwapchain, &actualImages, m_Impl->vkSwapchainImages.data());

        m_SwapchainTextures.clear();
        for (uint32_t i = 0; i < actualImages; ++i)
        {
            nvrhi::TextureDesc desc;
            desc.width = m_Width;
            desc.height = m_Height;
            desc.format = m_BackbufferFormat;
            desc.debugName = "SwapchainBackbuffer_" + std::to_string(i);
            desc.isRenderTarget = true;
            desc.initialState = nvrhi::ResourceStates::Present;
            desc.keepInitialState = true;

            nvrhi::TextureHandle tex = m_NvrhiDevice->createHandleForNativeTexture(
                nvrhi::ObjectTypes::VK_Image,
                nvrhi::Object(m_Impl->vkSwapchainImages[i]),
                desc);
            m_SwapchainTextures.push_back(tex);
        }

        VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        m_Impl->acquireSemaphores.resize(actualImages);
        m_Impl->presentSemaphores.resize(actualImages);
        for (uint32_t i = 0; i < actualImages; ++i)
        {
            vkCreateSemaphore(m_Impl->vkDevice, &semInfo, nullptr, &m_Impl->acquireSemaphores[i]);
            vkCreateSemaphore(m_Impl->vkDevice, &semInfo, nullptr, &m_Impl->presentSemaphores[i]);
        }
        m_Impl->semaphoreIndex = 0;
    }

    void RHIDevice::DestroyVulkanSwapchain()
    {
        if (!m_Impl || !m_Impl->vkDevice) return;
        vkDeviceWaitIdle(m_Impl->vkDevice);

        m_SwapchainFramebuffers.clear();
        m_DepthTexture = nullptr;
        m_SwapchainTextures.clear();

        for (VkSemaphore s : m_Impl->acquireSemaphores)
            if (s) vkDestroySemaphore(m_Impl->vkDevice, s, nullptr);
        for (VkSemaphore s : m_Impl->presentSemaphores)
            if (s) vkDestroySemaphore(m_Impl->vkDevice, s, nullptr);
        m_Impl->acquireSemaphores.clear();
        m_Impl->presentSemaphores.clear();

        if (m_Impl->vkSwapchain)
        {
            vkDestroySwapchainKHR(m_Impl->vkDevice, m_Impl->vkSwapchain, nullptr);
            m_Impl->vkSwapchain = VK_NULL_HANDLE;
        }
    }

    void RHIDevice::ShutdownVulkan()
    {
        DestroyVulkanSwapchain();
        m_Impl->vkNvrhiDevice = nullptr;

        if (m_Impl->vkDevice)
        {
            vkDestroyDevice(m_Impl->vkDevice, nullptr);
            m_Impl->vkDevice = VK_NULL_HANDLE;
        }
        if (m_Impl->vkSurface)
        {
            vkDestroySurfaceKHR(m_Impl->vkInstance, m_Impl->vkSurface, nullptr);
            m_Impl->vkSurface = VK_NULL_HANDLE;
        }
        if (m_Impl->vkInstance)
        {
            vkDestroyInstance(m_Impl->vkInstance, nullptr);
            m_Impl->vkInstance = VK_NULL_HANDLE;
        }
    }

#ifdef _WIN32
    bool RHIDevice::InitD3D11(Window* window, bool enableValidation)
    {
        HWND hwnd = static_cast<HWND>(window->GetWin32Handle());
        if (!hwnd) return false;

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = m_Width;
        sd.BufferDesc.Height = m_Height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL createdLevel{};

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            featureLevels,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &m_Impl->dxgiSwapChain,
            &m_Impl->d3dDevice,
            &createdLevel,
            &m_Impl->d3dContext);

        if (FAILED(hr))
        {
            Log::Error("D3D11CreateDeviceAndSwapChain failed.");
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        if (SUCCEEDED(m_Impl->d3dDevice.As(&dxgiDevice)))
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
            if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter)))
            {
                DXGI_ADAPTER_DESC ad{};
                adapter->GetDesc(&ad);
                char nameBuf[256]{};
                WideCharToMultiByte(CP_UTF8, 0, ad.Description, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
                m_GPUName = nameBuf;
            }
        }

        nvrhi::d3d11::DeviceDesc devDesc{};
        devDesc.messageCallback = &s_MessageCallback;
        devDesc.context = m_Impl->d3dContext.Get();

        m_RawDevice = nvrhi::d3d11::createDevice(devDesc);
        if (!m_RawDevice) return false;

        if (enableValidation)
            m_NvrhiDevice = nvrhi::validation::createValidationLayer(m_RawDevice);
        else
            m_NvrhiDevice = m_RawDevice;

        m_BackbufferFormat = nvrhi::Format::RGBA8_UNORM;
        CreateD3D11SwapchainResources(m_Width, m_Height);
        CreateDepthAndFramebuffers();
        return true;
    }

    void RHIDevice::CreateD3D11SwapchainResources(uint32_t width, uint32_t height)
    {
        m_Width = std::max(1u, width);
        m_Height = std::max(1u, height);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        m_Impl->dxgiSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

        nvrhi::TextureDesc desc;
        desc.width = m_Width;
        desc.height = m_Height;
        desc.format = m_BackbufferFormat;
        desc.debugName = "D3D11SwapchainBackbuffer";
        desc.isRenderTarget = true;
        desc.initialState = nvrhi::ResourceStates::RenderTarget;
        desc.keepInitialState = true;

        nvrhi::TextureHandle tex = m_NvrhiDevice->createHandleForNativeTexture(
            nvrhi::ObjectTypes::D3D11_Resource,
            nvrhi::Object(backBuffer.Get()),
            desc);

        m_SwapchainTextures.clear();
        m_SwapchainTextures.push_back(tex);
        m_CurrentBackbufferIndex = 0;
    }

    void RHIDevice::DestroyD3D11SwapchainResources()
    {
        m_SwapchainFramebuffers.clear();
        m_DepthTexture = nullptr;
        m_SwapchainTextures.clear();
    }

    void RHIDevice::ShutdownD3D11()
    {
        DestroyD3D11SwapchainResources();
        m_Impl->dxgiSwapChain.Reset();
        m_Impl->d3dContext.Reset();
        m_Impl->d3dDevice.Reset();
    }
#endif

    void RHIDevice::CreateDepthAndFramebuffers()
    {
        nvrhi::TextureDesc depthDesc;
        depthDesc.width = m_Width;
        depthDesc.height = m_Height;
        depthDesc.format = m_DepthFormat;
        depthDesc.isRenderTarget = true;
        depthDesc.isShaderResource = false;
        depthDesc.debugName = "MainDepthBuffer";
        depthDesc.initialState = nvrhi::ResourceStates::DepthWrite;
        depthDesc.keepInitialState = true;
        depthDesc.useClearValue = true;
        depthDesc.clearValue = nvrhi::Color(1.0f, 0.0f, 0.0f, 0.0f);

        m_DepthTexture = m_NvrhiDevice->createTexture(depthDesc);

        m_SwapchainFramebuffers.clear();
        for (auto& colorTex : m_SwapchainTextures)
        {
            nvrhi::FramebufferDesc fbDesc;
            fbDesc.addColorAttachment(colorTex);
            fbDesc.setDepthAttachment(m_DepthTexture);
            m_SwapchainFramebuffers.push_back(m_NvrhiDevice->createFramebuffer(fbDesc));
        }
    }

    void RHIDevice::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        WaitForIdle();

        if (m_ActiveBackend == GraphicsBackend::Vulkan)
        {
            DestroyVulkanSwapchain();
            CreateVulkanSwapchain(width, height);
            CreateDepthAndFramebuffers();
        }
#ifdef _WIN32
        else if (m_ActiveBackend == GraphicsBackend::D3D11)
        {
            DestroyD3D11SwapchainResources();
            m_Impl->dxgiSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
            CreateD3D11SwapchainResources(width, height);
            CreateDepthAndFramebuffers();
        }
#endif
    }

    bool RHIDevice::BeginFrame()
    {
        if (m_ActiveBackend == GraphicsBackend::Vulkan)
        {
            VkSemaphore acquireSem = m_Impl->acquireSemaphores[m_Impl->semaphoreIndex];
            VkResult res = vkAcquireNextImageKHR(
                m_Impl->vkDevice,
                m_Impl->vkSwapchain,
                UINT64_MAX,
                acquireSem,
                VK_NULL_HANDLE,
                &m_CurrentBackbufferIndex);

            if (res == VK_ERROR_OUT_OF_DATE_KHR)
            {
                Resize(m_Window->GetWidth(), m_Window->GetHeight());
                return false;
            }
            return (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);
        }

        m_CurrentBackbufferIndex = 0;
        return true;
    }

    void RHIDevice::FinishFrameAndPresent(nvrhi::ICommandList* commandList)
    {
        if (m_ActiveBackend == GraphicsBackend::Vulkan)
        {
            VkSemaphore acquireSem = m_Impl->acquireSemaphores[m_Impl->semaphoreIndex];
            VkSemaphore presentSem = m_Impl->presentSemaphores[m_CurrentBackbufferIndex];

            m_Impl->vkNvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, acquireSem, 0);
            m_Impl->vkNvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, presentSem, 0);

            m_NvrhiDevice->executeCommandList(commandList);

            VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &presentSem;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &m_Impl->vkSwapchain;
            presentInfo.pImageIndices = &m_CurrentBackbufferIndex;

            VkResult res = vkQueuePresentKHR(m_Impl->vkGraphicsQueue, &presentInfo);
            if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
            {
                Resize(m_Window->GetWidth(), m_Window->GetHeight());
            }
            else
            {
                vkQueueWaitIdle(m_Impl->vkGraphicsQueue);
            }

            m_NvrhiDevice->runGarbageCollection();
            m_Impl->semaphoreIndex = (m_Impl->semaphoreIndex + 1) % m_Impl->acquireSemaphores.size();
        }
#ifdef _WIN32
        else if (m_ActiveBackend == GraphicsBackend::D3D11)
        {
            m_NvrhiDevice->executeCommandList(commandList);
            m_Impl->dxgiSwapChain->Present(1, 0);
            m_NvrhiDevice->runGarbageCollection();
        }
#endif
        ++m_FrameIndex;
    }

    void RHIDevice::WaitForIdle()
    {
        if (m_NvrhiDevice)
        {
            m_NvrhiDevice->waitForIdle();
            m_NvrhiDevice->runGarbageCollection();
        }
    }

    nvrhi::IFramebuffer* RHIDevice::GetCurrentFramebuffer() const
    {
        if (m_CurrentBackbufferIndex < m_SwapchainFramebuffers.size())
            return m_SwapchainFramebuffers[m_CurrentBackbufferIndex];
        return nullptr;
    }

    nvrhi::ITexture* RHIDevice::GetCurrentBackbuffer() const
    {
        if (m_CurrentBackbufferIndex < m_SwapchainTextures.size())
            return m_SwapchainTextures[m_CurrentBackbufferIndex];
        return nullptr;
    }

    std::string RHIDevice::GetBackendName() const
    {
        return (m_ActiveBackend == GraphicsBackend::Vulkan) ? "Vulkan (NVRHI)" : "Direct3D 11 (NVRHI)";
    }

    nvrhi::ShaderHandle RHIDevice::CreateShader(
        nvrhi::ShaderType stage,
        const std::string& debugName,
        const std::string& entryPoint,
        const void* spirvData,
        size_t spirvByteSize,
        const std::string& hlslFilePath,
        const char* embeddedHlslFallback)
    {
        nvrhi::ShaderDesc desc(stage);
        desc.debugName = debugName;
        desc.entryName = entryPoint;

        if (m_ActiveBackend == GraphicsBackend::Vulkan)
        {
            return m_NvrhiDevice->createShader(desc, spirvData, spirvByteSize);
        }

#ifdef _WIN32
        if (m_ActiveBackend == GraphicsBackend::D3D11)
        {
            std::string hlslSource;
            std::ifstream in(hlslFilePath, std::ios::in | std::ios::binary);
            if (in)
            {
                std::ostringstream ss;
                ss << in.rdbuf();
                hlslSource = ss.str();
            }
            else if (embeddedHlslFallback)
            {
                hlslSource = embeddedHlslFallback;
            }

            if (hlslSource.empty())
            {
                Log::Error("Failed to find HLSL source for D3D11 shader: " + debugName);
                return nullptr;
            }

            const char* target = (stage == nvrhi::ShaderType::Vertex) ? "vs_5_0" : "ps_5_0";
            Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
            Microsoft::WRL::ComPtr<ID3DBlob> errors;
            HRESULT hr = D3DCompile(
                hlslSource.data(),
                hlslSource.size(),
                debugName.c_str(),
                nullptr,
                nullptr,
                entryPoint.c_str(),
                target,
                D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
                0,
                &bytecode,
                &errors);

            if (FAILED(hr))
            {
                std::string errStr = errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Unknown D3DCompile error";
                Log::Error("D3DCompile failed for " + debugName + ": " + errStr);
                return nullptr;
            }

            return m_NvrhiDevice->createShader(desc, bytecode->GetBufferPointer(), bytecode->GetBufferSize());
        }
#endif
        return nullptr;
    }

    OffscreenTarget RHIDevice::CreateOffscreenTarget(uint32_t width, uint32_t height)
    {
        OffscreenTarget target;
        target.width = std::max(64u, width);
        target.height = std::max(64u, height);

        nvrhi::TextureDesc colorDesc;
        colorDesc.width = target.width;
        colorDesc.height = target.height;
        colorDesc.format = nvrhi::Format::RGBA8_UNORM;
        colorDesc.debugName = "OffscreenViewportColor";
        colorDesc.isRenderTarget = true;
        colorDesc.isShaderResource = true;
        colorDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        colorDesc.keepInitialState = true;
        colorDesc.useClearValue = true;
        colorDesc.clearValue = nvrhi::Color(0.05f, 0.07f, 0.12f, 1.0f);
        target.colorTexture = m_NvrhiDevice->createTexture(colorDesc);

        nvrhi::TextureDesc depthDesc;
        depthDesc.width = target.width;
        depthDesc.height = target.height;
        depthDesc.format = m_DepthFormat;
        depthDesc.debugName = "OffscreenViewportDepth";
        depthDesc.isRenderTarget = true;
        depthDesc.isShaderResource = false;
        depthDesc.initialState = nvrhi::ResourceStates::DepthWrite;
        depthDesc.keepInitialState = true;
        depthDesc.useClearValue = true;
        depthDesc.clearValue = nvrhi::Color(1.0f, 0.0f, 0.0f, 0.0f);
        target.depthTexture = m_NvrhiDevice->createTexture(depthDesc);

        nvrhi::FramebufferDesc fbDesc;
        fbDesc.addColorAttachment(target.colorTexture);
        fbDesc.setDepthAttachment(target.depthTexture);
        target.framebuffer = m_NvrhiDevice->createFramebuffer(fbDesc);

        return target;
    }

    bool RHIDevice::SaveTextureToBMP(nvrhi::ITexture* texture, const std::string& filePath)
    {
        if (!texture || !m_NvrhiDevice)
            return false;

        const nvrhi::TextureDesc& srcDesc = texture->getDesc();
        nvrhi::TextureDesc stagingDesc = srcDesc;
        stagingDesc.isRenderTarget = false;
        stagingDesc.isShaderResource = false;
        stagingDesc.isUAV = false;
        stagingDesc.initialState = nvrhi::ResourceStates::Common;
        stagingDesc.keepInitialState = false;
        stagingDesc.debugName = "ScreenshotStaging";

        nvrhi::StagingTextureHandle staging = m_NvrhiDevice->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
        if (!staging)
            return false;

        nvrhi::CommandListHandle cmd = m_NvrhiDevice->createCommandList();
        cmd->open();
        cmd->copyTexture(staging, nvrhi::TextureSlice(), texture, nvrhi::TextureSlice());
        cmd->close();
        m_NvrhiDevice->executeCommandList(cmd);
        WaitForIdle();

        size_t rowPitch = 0;
        const uint8_t* mapped = static_cast<const uint8_t*>(
            m_NvrhiDevice->mapStagingTexture(staging, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch));
        if (!mapped)
            return false;

        uint32_t w = srcDesc.width;
        uint32_t h = srcDesc.height;
        bool isBGRA = (srcDesc.format == nvrhi::Format::BGRA8_UNORM || srcDesc.format == nvrhi::Format::SBGRA8_UNORM);

        uint32_t bmpRowStride = (w * 3 + 3) & ~3u;
        uint32_t pixelDataSize = bmpRowStride * h;

#pragma pack(push, 1)
        struct BMPHeader
        {
            uint16_t bfType = 0x4D42;
            uint32_t bfSize = 0;
            uint16_t bfReserved1 = 0;
            uint16_t bfReserved2 = 0;
            uint32_t bfOffBits = 54;
            uint32_t biSize = 40;
            int32_t  biWidth = 0;
            int32_t  biHeight = 0;
            uint16_t biPlanes = 1;
            uint16_t biBitCount = 24;
            uint32_t biCompression = 0;
            uint32_t biSizeImage = 0;
            int32_t  biXPelsPerMeter = 2835;
            int32_t  biYPelsPerMeter = 2835;
            uint32_t biClrUsed = 0;
            uint32_t biClrImportant = 0;
        } header;
#pragma pack(pop)

        header.bfSize = 54 + pixelDataSize;
        header.biWidth = static_cast<int32_t>(w);
        header.biHeight = static_cast<int32_t>(h); // bottom-up BMP
        header.biSizeImage = pixelDataSize;

        std::ofstream out(filePath, std::ios::out | std::ios::binary);
        if (!out)
        {
            m_NvrhiDevice->unmapStagingTexture(staging);
            return false;
        }

        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        std::vector<uint8_t> rowBuf(bmpRowStride, 0);

        for (int32_t y = static_cast<int32_t>(h) - 1; y >= 0; --y)
        {
            const uint8_t* srcRow = mapped + static_cast<size_t>(y) * rowPitch;
            for (uint32_t x = 0; x < w; ++x)
            {
                uint8_t c0 = srcRow[x * 4 + 0];
                uint8_t c1 = srcRow[x * 4 + 1];
                uint8_t c2 = srcRow[x * 4 + 2];
                // BMP expects B, G, R
                if (isBGRA)
                {
                    rowBuf[x * 3 + 0] = c0;
                    rowBuf[x * 3 + 1] = c1;
                    rowBuf[x * 3 + 2] = c2;
                }
                else
                {
                    rowBuf[x * 3 + 0] = c2;
                    rowBuf[x * 3 + 1] = c1;
                    rowBuf[x * 3 + 2] = c0;
                }
            }
            out.write(reinterpret_cast<const char*>(rowBuf.data()), bmpRowStride);
        }

        m_NvrhiDevice->unmapStagingTexture(staging);
        Log::RHI("Saved screenshot to: " + filePath);
        return true;
    }
}
