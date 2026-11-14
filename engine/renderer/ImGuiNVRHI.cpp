#include "ImGuiNVRHI.h"
#include "../rhi/EmbeddedShaders.h"
#include "../core/Window.h"
#include "../core/Log.h"

#include <nvrhi/utils.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <glm/glm.hpp>
#include <algorithm>

namespace Hargon
{
    ImGuiNVRHI::~ImGuiNVRHI()
    {
        Shutdown();
    }

    static void ApplyHargonEditorStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 3.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.FramePadding = ImVec2(7.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 5.0f);
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg]             = ImVec4(0.09f, 0.11f, 0.15f, 0.98f);
        colors[ImGuiCol_ChildBg]              = ImVec4(0.07f, 0.09f, 0.12f, 1.00f);
        colors[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.13f, 0.18f, 0.98f);
        colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.27f, 0.36f, 0.65f);
        colors[ImGuiCol_FrameBg]              = ImVec4(0.13f, 0.17f, 0.23f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.19f, 0.26f, 0.36f, 1.00f);
        colors[ImGuiCol_FrameBgActive]        = ImVec4(0.23f, 0.33f, 0.46f, 1.00f);
        colors[ImGuiCol_TitleBg]              = ImVec4(0.06f, 0.08f, 0.11f, 1.00f);
        colors[ImGuiCol_TitleBgActive]        = ImVec4(0.10f, 0.15f, 0.22f, 1.00f);
        colors[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
        colors[ImGuiCol_CheckMark]            = ImVec4(0.26f, 0.82f, 0.98f, 1.00f);
        colors[ImGuiCol_SliderGrab]           = ImVec4(0.22f, 0.68f, 0.92f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.32f, 0.85f, 1.00f, 1.00f);
        colors[ImGuiCol_Button]               = ImVec4(0.16f, 0.28f, 0.42f, 1.00f);
        colors[ImGuiCol_ButtonHovered]        = ImVec4(0.22f, 0.42f, 0.62f, 1.00f);
        colors[ImGuiCol_ButtonActive]         = ImVec4(0.28f, 0.55f, 0.80f, 1.00f);
        colors[ImGuiCol_Header]               = ImVec4(0.15f, 0.26f, 0.38f, 0.85f);
        colors[ImGuiCol_HeaderHovered]        = ImVec4(0.22f, 0.38f, 0.55f, 0.90f);
        colors[ImGuiCol_HeaderActive]         = ImVec4(0.26f, 0.48f, 0.68f, 1.00f);
        colors[ImGuiCol_Separator]            = ImVec4(0.20f, 0.28f, 0.38f, 0.65f);
        colors[ImGuiCol_Tab]                  = ImVec4(0.11f, 0.15f, 0.21f, 1.00f);
        colors[ImGuiCol_TabHovered]           = ImVec4(0.22f, 0.38f, 0.56f, 1.00f);
        colors[ImGuiCol_TabSelected]          = ImVec4(0.17f, 0.30f, 0.45f, 1.00f);
        colors[ImGuiCol_DockingPreview]       = ImVec4(0.22f, 0.68f, 0.95f, 0.50f);
    }

    bool ImGuiNVRHI::Init(RHIDevice* rhi, Window* window)
    {
        m_RHI = rhi;
        m_Device = rhi->GetDevice();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = nullptr; // Managed cleanly via code

        ApplyHargonEditorStyle();

        ImGui_ImplGlfw_InitForOther(window->GetNativeWindow(), true);

        // Compile ImGui shaders
        m_VS = m_RHI->CreateShader(
            nvrhi::ShaderType::Vertex, "ImGui_VS", "VSMain",
            g_ImGui_VS_SPV, sizeof(g_ImGui_VS_SPV), "assets/shaders/ImGuiShader.hlsl", Shaders::kImGuiHLSL);
        m_PS = m_RHI->CreateShader(
            nvrhi::ShaderType::Pixel, "ImGui_PS", "PSMain",
            g_ImGui_PS_SPV, sizeof(g_ImGui_PS_SPV), "assets/shaders/ImGuiShader.hlsl", Shaders::kImGuiHLSL);

        if (!m_VS || !m_PS)
        {
            Log::Error("Failed to create ImGui NVRHI shaders!");
            return false;
        }

        nvrhi::VertexAttributeDesc attrs[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setOffset(offsetof(ImDrawVert, pos))
                .setElementStride(sizeof(ImDrawVert)),
            nvrhi::VertexAttributeDesc()
                .setName("TEXCOORD")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setOffset(offsetof(ImDrawVert, uv))
                .setElementStride(sizeof(ImDrawVert)),
            nvrhi::VertexAttributeDesc()
                .setName("COLOR")
                .setFormat(nvrhi::Format::RGBA8_UNORM)
                .setOffset(offsetof(ImDrawVert, col))
                .setElementStride(sizeof(ImDrawVert))
        };
        m_InputLayout = m_Device->createInputLayout(attrs, 3, m_VS);

        m_ConstantBuffer = m_Device->createBuffer(
            nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(glm::mat4), "ImGuiCB", 32));

        // Upload Font Atlas
        unsigned char* pixels = nullptr;
        int fontW = 0, fontH = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &fontW, &fontH);

        nvrhi::TextureDesc fontDesc;
        fontDesc.width = static_cast<uint32_t>(fontW);
        fontDesc.height = static_cast<uint32_t>(fontH);
        fontDesc.format = nvrhi::Format::RGBA8_UNORM;
        fontDesc.debugName = "ImGuiFontAtlas";
        fontDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        fontDesc.keepInitialState = true;
        m_FontTexture = m_Device->createTexture(fontDesc);

        nvrhi::CommandListHandle cmd = m_Device->createCommandList();
        cmd->open();
        cmd->writeTexture(m_FontTexture, 0, 0, pixels, static_cast<size_t>(fontW) * 4);
        cmd->close();
        m_Device->executeCommandList(cmd);
        m_RHI->WaitForIdle();

        io.Fonts->SetTexID(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(m_FontTexture.Get())));

        nvrhi::SamplerDesc sampDesc;
        sampDesc.setAllFilters(true);
        sampDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Wrap);
        m_Sampler = m_Device->createSampler(sampDesc);

        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.bindings = {
            nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
            nvrhi::BindingLayoutItem::Texture_SRV(0),
            nvrhi::BindingLayoutItem::Sampler(0)
        };
        m_BindingLayout = m_Device->createBindingLayout(layoutDesc);

        // Create Vertex & Index Buffers
        nvrhi::BufferDesc vbDesc;
        vbDesc.byteSize = m_MaxVertices * sizeof(ImDrawVert);
        vbDesc.isVertexBuffer = true;
        vbDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
        vbDesc.keepInitialState = true;
        vbDesc.debugName = "ImGuiVB";
        m_VertexBuffer = m_Device->createBuffer(vbDesc);

        nvrhi::BufferDesc ibDesc;
        ibDesc.byteSize = m_MaxIndices * sizeof(ImDrawIdx);
        ibDesc.isIndexBuffer = true;
        ibDesc.initialState = nvrhi::ResourceStates::IndexBuffer;
        ibDesc.keepInitialState = true;
        ibDesc.debugName = "ImGuiIB";
        m_IndexBuffer = m_Device->createBuffer(ibDesc);

        m_Initialized = true;
        return true;
    }

    void ImGuiNVRHI::Shutdown()
    {
        if (!m_Initialized)
            return;

        m_BindingSets.clear();
        m_Pipeline = nullptr;
        m_VertexBuffer = nullptr;
        m_IndexBuffer = nullptr;
        m_BindingLayout = nullptr;
        m_Sampler = nullptr;
        m_FontTexture = nullptr;
        m_ConstantBuffer = nullptr;
        m_InputLayout = nullptr;
        m_VS = nullptr;
        m_PS = nullptr;

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_Initialized = false;
    }

    void ImGuiNVRHI::InvalidateTextureCache()
    {
        m_BindingSets.clear();
    }

    void ImGuiNVRHI::BeginFrame()
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiNVRHI::EnsurePipeline(nvrhi::IFramebuffer* targetFramebuffer)
    {
        const nvrhi::FramebufferInfo& fbInfo = targetFramebuffer->getFramebufferInfo();
        if (m_HasCachedFbInfo && m_CachedFbInfo == fbInfo && m_Pipeline)
            return;

        m_CachedFbInfo = fbInfo;
        m_HasCachedFbInfo = true;

        nvrhi::GraphicsPipelineDesc desc;
        desc.primType = nvrhi::PrimitiveType::TriangleList;
        desc.inputLayout = m_InputLayout;
        desc.VS = m_VS;
        desc.PS = m_PS;
        desc.bindingLayouts = { m_BindingLayout };

        desc.renderState.rasterState
            .setCullNone()
            .setScissorEnable(true)
            .setDepthClipEnable(true);

        desc.renderState.depthStencilState
            .setDepthTestEnable(false)
            .setDepthWriteEnable(false);

        desc.renderState.blendState.targets[0]
            .setBlendEnable(true)
            .setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
            .setDestBlend(nvrhi::BlendFactor::InvSrcAlpha)
            .setBlendOp(nvrhi::BlendOp::Add)
            .setSrcBlendAlpha(nvrhi::BlendFactor::One)
            .setDestBlendAlpha(nvrhi::BlendFactor::InvSrcAlpha)
            .setBlendOpAlpha(nvrhi::BlendOp::Add);

        m_Pipeline = m_Device->createGraphicsPipeline(desc, targetFramebuffer);
    }

    nvrhi::IBindingSet* ImGuiNVRHI::GetOrCreateBindingSet(nvrhi::ITexture* texture)
    {
        if (!texture)
            texture = m_FontTexture.Get();

        auto it = m_BindingSets.find(texture);
        if (it != m_BindingSets.end())
            return it->second.Get();

        nvrhi::BindingSetDesc bset;
        bset.bindings = {
            nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, texture),
            nvrhi::BindingSetItem::Sampler(0, m_Sampler)
        };
        nvrhi::BindingSetHandle handle = m_Device->createBindingSet(bset, m_BindingLayout);
        m_BindingSets[texture] = handle;
        return handle.Get();
    }

    void ImGuiNVRHI::Render(nvrhi::ICommandList* cmd, nvrhi::IFramebuffer* targetFramebuffer)
    {
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (!drawData || drawData->TotalVtxCount <= 0 || !targetFramebuffer)
            return;

        int fbWidth = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
        int fbHeight = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
        if (fbWidth <= 0 || fbHeight <= 0)
            return;

        EnsurePipeline(targetFramebuffer);

        // Grow buffers if needed
        if (static_cast<uint32_t>(drawData->TotalVtxCount) > m_MaxVertices)
        {
            m_MaxVertices = static_cast<uint32_t>(drawData->TotalVtxCount) + 16384;
            nvrhi::BufferDesc vbDesc;
            vbDesc.byteSize = m_MaxVertices * sizeof(ImDrawVert);
            vbDesc.isVertexBuffer = true;
            vbDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
            vbDesc.keepInitialState = true;
            vbDesc.debugName = "ImGuiVB";
            m_VertexBuffer = m_Device->createBuffer(vbDesc);
        }

        if (static_cast<uint32_t>(drawData->TotalIdxCount) > m_MaxIndices)
        {
            m_MaxIndices = static_cast<uint32_t>(drawData->TotalIdxCount) + 32768;
            nvrhi::BufferDesc ibDesc;
            ibDesc.byteSize = m_MaxIndices * sizeof(ImDrawIdx);
            ibDesc.isIndexBuffer = true;
            ibDesc.initialState = nvrhi::ResourceStates::IndexBuffer;
            ibDesc.keepInitialState = true;
            ibDesc.debugName = "ImGuiIB";
            m_IndexBuffer = m_Device->createBuffer(ibDesc);
        }

        std::vector<ImDrawVert> allVertices(drawData->TotalVtxCount);
        std::vector<ImDrawIdx> allIndices(drawData->TotalIdxCount);
        ImDrawVert* vtxDst = allVertices.data();
        ImDrawIdx* idxDst = allIndices.data();

        for (int n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            std::memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            std::memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmdList->VtxBuffer.Size;
            idxDst += cmdList->IdxBuffer.Size;
        }

        cmd->writeBuffer(m_VertexBuffer, allVertices.data(), allVertices.size() * sizeof(ImDrawVert));
        cmd->writeBuffer(m_IndexBuffer, allIndices.data(), allIndices.size() * sizeof(ImDrawIdx));

        float L = drawData->DisplayPos.x;
        float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
        float T = drawData->DisplayPos.y;
        float B = drawData->DisplayPos.y + drawData->DisplaySize.y;

        // Row-major projection matrix for mul(float4(pos,0,1), u_ProjMatrix)
        glm::mat4 proj(0.0f);
        proj[0][0] = 2.0f / (R - L);
        proj[1][1] = 2.0f / (T - B);
        proj[2][2] = 0.5f;
        proj[3][0] = (R + L) / (L - R);
        proj[3][1] = (T + B) / (B - T);
        proj[3][2] = 0.5f;
        proj[3][3] = 1.0f;
        cmd->writeBuffer(m_ConstantBuffer, &proj, sizeof(proj));

        ImVec2 clipOff = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;

        int globalVtxOffset = 0;
        int globalIdxOffset = 0;

        for (int n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            for (int cmd_i = 0; cmd_i < cmdList->CmdBuffer.Size; ++cmd_i)
            {
                const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmd_i];
                if (pcmd->UserCallback != nullptr)
                {
                    pcmd->UserCallback(cmdList, pcmd);
                }
                else
                {
                    ImVec2 clipMin((pcmd->ClipRect.x - clipOff.x) * clipScale.x, (pcmd->ClipRect.y - clipOff.y) * clipScale.y);
                    ImVec2 clipMax((pcmd->ClipRect.z - clipOff.x) * clipScale.x, (pcmd->ClipRect.w - clipOff.y) * clipScale.y);

                    clipMin.x = std::max(0.0f, clipMin.x);
                    clipMin.y = std::max(0.0f, clipMin.y);
                    clipMax.x = std::min(static_cast<float>(fbWidth), clipMax.x);
                    clipMax.y = std::min(static_cast<float>(fbHeight), clipMax.y);

                    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                        continue;

                    nvrhi::ITexture* tex = reinterpret_cast<nvrhi::ITexture*>(static_cast<uintptr_t>(pcmd->GetTexID()));
                    nvrhi::IBindingSet* bindingSet = GetOrCreateBindingSet(tex);

                    nvrhi::GraphicsState state;
                    state.pipeline = m_Pipeline;
                    state.framebuffer = targetFramebuffer;
                    state.bindings = { bindingSet };
                    state.vertexBuffers = { { m_VertexBuffer, 0, 0 } };
                    state.indexBuffer = {
                        m_IndexBuffer,
                        sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT,
                        0
                    };
                    state.viewport.addViewport(nvrhi::Viewport(static_cast<float>(fbWidth), static_cast<float>(fbHeight)));
                    state.viewport.addScissorRect(nvrhi::Rect(
                        static_cast<int>(clipMin.x),
                        static_cast<int>(clipMax.x),
                        static_cast<int>(clipMin.y),
                        static_cast<int>(clipMax.y)));

                    cmd->setGraphicsState(state);

                    nvrhi::DrawArguments args;
                    args.vertexCount = pcmd->ElemCount;
                    args.startIndexLocation = pcmd->IdxOffset + globalIdxOffset;
                    args.startVertexLocation = pcmd->VtxOffset + globalVtxOffset;
                    cmd->drawIndexed(args);
                }
            }
            globalIdxOffset += cmdList->IdxBuffer.Size;
            globalVtxOffset += cmdList->VtxBuffer.Size;
        }
    }
}
