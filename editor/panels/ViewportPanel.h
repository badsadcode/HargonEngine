#pragma once
#include "../EditorContext.h"
#include "../../engine/rhi/RHIDevice.h"
#include <glm/glm.hpp>
#include <imgui.h>
#include <string>
#include <vector>

namespace Hargon
{
    class ViewportPanel
    {
    public:
        void Shutdown(EditorContext& ctx);

        void SyncRenderTargetIfNeeded(EditorContext& ctx);
        nvrhi::IFramebuffer* GetFramebuffer() const { return m_ViewportRT.framebuffer; }
        nvrhi::ITexture* GetColorTexture() const { return m_ViewportRT.colorTexture; }
        uint32_t GetWidth() const { return m_ViewportRT.width > 0 ? m_ViewportRT.width : m_DesiredWidth; }
        uint32_t GetHeight() const { return m_ViewportRT.height > 0 ? m_ViewportRT.height : m_DesiredHeight; }

        void RenderSceneToTarget(EditorContext& ctx, nvrhi::ICommandList* cmd, float elapsedTime);
        void Draw(EditorContext& ctx, float dt);

        // Accurate ray calculation matching true viewport aspect ratio
        static void ComputeViewportRay(
            const CameraView& cam,
            const ImVec2& screenMousePos,
            const ImVec2& vpMin,
            const ImVec2& vpSize,
            glm::vec3& outOrigin,
            glm::vec3& outDir);

    private:
        void HandleNavigationAndPicking(EditorContext& ctx, float dt, const ImVec2& vpMin, const ImVec2& vpSize, bool vpHovered);
        void DrawViewportOverlay(EditorContext& ctx, const ImVec2& vpMin, const ImVec2& vpSize);
        glm::vec3 GetGizmoAxisVector(const Entity& primary, int axisIdx, bool localSpace, const Scene& scene) const;

    private:
        OffscreenTarget m_ViewportRT{};
        uint32_t m_DesiredWidth = 960;
        uint32_t m_DesiredHeight = 540;
        ImVec2 m_LastVpMin{ 0.0f, 0.0f };
        ImVec2 m_LastVpSize{ 960.0f, 540.0f };

        // Camera navigation & orbit state
        bool m_RightMouseNavigating = false;
        bool m_OrbitingCamera = false;
        bool m_PanningCamera = false;
        float m_OrbitDistance = 12.0f;
        glm::vec3 m_OrbitPivot{ 0.0f };
        ImVec2 m_LastMousePos{ 0.0f, 0.0f };

        // Gizmo interaction state
        int m_HoveredGizmoAxis = -1;
        int m_ActiveGizmoAxis = -1;
        bool m_GizmoShiftDuplicated = false;
        float m_GizmoDragStartDist = 0.0f;
        glm::vec3 m_GizmoAxisVec{ 1.0f, 0.0f, 0.0f };
        glm::vec3 m_GizmoCenter{ 0.0f };
        std::vector<Entity> m_GizmoBeforeSnapshot;
        std::vector<EntityID> m_GizmoTargetIds;
        std::vector<TransformComponent> m_GizmoStartTransforms;
        std::vector<glm::vec3> m_GizmoStartWorldPositions;
        std::vector<EntityID> m_GizmoBeforeSel;
        EntityID m_GizmoBeforePrimary = kInvalidEntity;
    };
}
