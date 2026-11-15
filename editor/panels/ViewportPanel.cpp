#include "ViewportPanel.h"
#include "../commands/EditorCommandSystem.h"
#include "../../engine/runtime/GameSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Hargon
{
    void ViewportPanel::Shutdown(EditorContext& ctx)
    {
        if (ctx.rhi)
            ctx.rhi->WaitForIdle();
        if (ctx.imguiLayer)
            ctx.imguiLayer->InvalidateTextureCache();
        m_ViewportRT = OffscreenTarget{};
    }

    void ViewportPanel::SyncRenderTargetIfNeeded(EditorContext& ctx)
    {
        if (!ctx.rhi) return;
        uint32_t tw = std::max(64u, m_DesiredWidth);
        uint32_t th = std::max(64u, m_DesiredHeight);
        if (m_ViewportRT.colorTexture && m_ViewportRT.width == tw && m_ViewportRT.height == th)
            return;

        ctx.rhi->WaitForIdle();
        if (ctx.imguiLayer)
            ctx.imguiLayer->InvalidateTextureCache();
        m_ViewportRT = ctx.rhi->CreateOffscreenTarget(tw, th);
    }

    void ViewportPanel::ComputeViewportRay(
        const CameraView& cam,
        const ImVec2& screenMousePos,
        const ImVec2& vpMin,
        const ImVec2& vpSize,
        glm::vec3& outOrigin,
        glm::vec3& outDir)
    {
        float w = std::max(1.0f, vpSize.x);
        float h = std::max(1.0f, vpSize.y);
        float ndcX = ((screenMousePos.x - vpMin.x) / w) * 2.0f - 1.0f;
        float ndcY = 1.0f - ((screenMousePos.y - vpMin.y) / h) * 2.0f;

        CameraView rayCam = cam;
        rayCam.aspectRatio = w / h;
        glm::mat4 invViewProj = glm::inverse(rayCam.GetViewProjectionMatrix());

        glm::vec4 nearPt = invViewProj * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
        glm::vec4 farPt  = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (std::abs(nearPt.w) > 1e-6f) nearPt /= nearPt.w;
        if (std::abs(farPt.w) > 1e-6f) farPt /= farPt.w;

        outOrigin = glm::vec3(nearPt);
        glm::vec3 delta = glm::vec3(farPt - nearPt);
        outDir = (glm::length(delta) > 1e-7f) ? glm::normalize(delta) : rayCam.forward;
    }

    glm::vec3 ViewportPanel::GetGizmoAxisVector(const Entity& primary, int axisIdx, bool localSpace, const Scene& scene) const
    {
        if (axisIdx < 0 || axisIdx > 2) return glm::vec3(1, 0, 0);
        if (!localSpace)
        {
            glm::vec3 ax(0.0f);
            ax[axisIdx] = 1.0f;
            return ax;
        }
        glm::mat3 basis = scene.GetWorldRotationBasis(primary);
        return glm::normalize(basis[axisIdx]);
    }

    void ViewportPanel::RenderSceneToTarget(EditorContext& ctx, nvrhi::ICommandList* cmd, float elapsedTime)
    {
        if (!m_ViewportRT.framebuffer || !ctx.renderer3D || !ctx.scene || !ctx.editorCamera)
            return;

        uint32_t w = std::max(1u, m_ViewportRT.width);
        uint32_t h = std::max(1u, m_ViewportRT.height);
        ctx.editorCamera->aspectRatio = static_cast<float>(w) / static_cast<float>(h);

        bool playing = (ctx.playState && *ctx.playState != PlayState::Edit);

        const bool drawGrid = !playing && ctx.viewportTools.drawGrid;
        const bool drawCol  = !playing && ctx.viewportTools.drawColliders;
        const EntityID sel  = !playing ? ctx.selectedEntity : kInvalidEntity;

        ctx.renderer3D->RenderScene(
            cmd,
            m_ViewportRT.framebuffer,
            *ctx.scene,
            *ctx.editorCamera,
            elapsedTime,
            drawGrid,
            sel,
            ctx.viewportTools.gizmoMode,
            m_HoveredGizmoAxis,
            drawCol,
            ctx.viewportTools.localSpace,
            (!playing && !ctx.selectedEntities.empty()) ? &ctx.selectedEntities : nullptr);
    }

    void ViewportPanel::Draw(EditorContext& ctx, float dt)
    {
        if (!ctx.showViewport)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool visible = ImGui::Begin(kWinViewport, &ctx.showViewport);
        ImGui::PopStyleVar();

        if (!visible)
        {
            ctx.viewportFocused = false;
            ctx.viewportHovered = false;
            ImGui::End();
            return;
        }

        ctx.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        ImVec2 avail = ImGui::GetContentRegionAvail();
        m_DesiredWidth = static_cast<uint32_t>(std::max(64.0f, avail.x));
        m_DesiredHeight = static_cast<uint32_t>(std::max(64.0f, avail.y));

        ImVec2 vpMin = ImGui::GetCursorScreenPos();
        ImVec2 vpSize(std::max(1.0f, avail.x), std::max(1.0f, avail.y));
        m_LastVpMin = vpMin;
        m_LastVpSize = vpSize;

        if (m_ViewportRT.colorTexture)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(m_ViewportRT.colorTexture.Get()), vpSize);
        }
        else
        {
            ImGui::Dummy(vpSize);
        }

        const bool vpHovered = ImGui::IsItemHovered();
        ctx.viewportHovered = vpHovered;

        // Drag-drop instantiation from Asset Browser onto 3D surface
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* accept = ImGui::AcceptDragDropPayload("HARGON_ASSET"))
            {
                std::string relPath(static_cast<const char*>(accept->Data));
                if (ctx.instantiateAssetInScene && ctx.scene && ctx.editorCamera)
                {
                    glm::vec3 rayO, rayD;
                    ComputeViewportRay(*ctx.editorCamera, ImGui::GetMousePos(), vpMin, vpSize, rayO, rayD);
                    glm::vec3 hitPos = rayO + rayD * 8.0f;
                    float hitD = 0.0f;
                    if (!ctx.scene->RaycastSurface(rayO, rayD, kInvalidEntity, hitD, hitPos, ctx.MakeBoundsLookup()))
                    {
                        if (std::abs(rayD.y) > 1e-4f)
                        {
                            float tPlane = -rayO.y / rayD.y;
                            if (tPlane > 0.5f && tPlane < 100.0f)
                                hitPos = rayO + rayD * tPlane;
                        }
                    }
                    ctx.instantiateAssetInScene(relPath, &hitPos);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Handle gameplay click-to-capture in FPS mode or Editor picking/gizmo/navigation
        bool playing = (ctx.playState && *ctx.playState == PlayState::Playing);
        if (playing)
        {
            if (vpHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                ctx.gameModule && *ctx.gameModule && (*ctx.gameModule)->WantsCursorCapture())
            {
                ctx.gameplayMouseLockActive = true;
            }
        }
        else if (!ctx.playState || *ctx.playState == PlayState::Edit)
        {
            HandleNavigationAndPicking(ctx, dt, vpMin, vpSize, vpHovered);
        }

        // Render HUD overlay if Playing/Paused
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (ctx.playState && *ctx.playState != PlayState::Edit && ctx.scene)
        {
            if (ctx.gameModule && *ctx.gameModule && ctx.buildModuleContext)
                (*ctx.gameModule)->OnRenderHUD(ctx.buildModuleContext(), vpMin, vpSize, dl);
            else
                GameSystem::RenderHUD(*ctx.scene, vpMin, vpSize, dl);
        }

        DrawViewportOverlay(ctx, vpMin, vpSize);

        ImGui::End();
    }

    void ViewportPanel::HandleNavigationAndPicking(EditorContext& ctx, float dt, const ImVec2& vpMin, const ImVec2& vpSize, bool vpHovered)
    {
        if (!ctx.scene || !ctx.editorCamera || !ctx.renderer3D)
            return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mousePos = io.MousePos;
        glm::vec2 mouseDelta(mousePos.x - m_LastMousePos.x, mousePos.y - m_LastMousePos.y);
        m_LastMousePos = mousePos;

        glm::vec3 rayO, rayD;
        ComputeViewportRay(*ctx.editorCamera, mousePos, vpMin, vpSize, rayO, rayD);

        // Update hovered gizmo axis when idle
        m_HoveredGizmoAxis = -1;
        if (m_ActiveGizmoAxis < 0 && vpHovered && ctx.selectedEntity != kInvalidEntity)
        {
            if (const Entity* sel = ctx.scene->GetEntity(ctx.selectedEntity))
            {
                glm::vec3 center = ctx.scene->GetWorldPosition(*sel);
                glm::mat3 basis = ctx.viewportTools.localSpace ? ctx.scene->GetWorldRotationBasis(*sel) : glm::mat3(1.0f);
                m_HoveredGizmoAxis = ctx.renderer3D->RaycastGizmo(center, *ctx.editorCamera, rayO, rayD, nullptr, basis);
            }
        }

        // Start right-click fly, middle-click pan, or Alt+LMB orbit
        if (vpHovered)
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                m_RightMouseNavigating = true;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
                m_PanningCamera = true;
            if (io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_OrbitingCamera = true;
                if (const Entity* sel = ctx.scene->GetEntity(ctx.selectedEntity))
                    m_OrbitPivot = ctx.scene->GetWorldPosition(*sel);
                else
                    m_OrbitPivot = ctx.editorCamera->position + ctx.editorCamera->forward * m_OrbitDistance;
                m_OrbitDistance = std::clamp(glm::length(ctx.editorCamera->position - m_OrbitPivot), 2.0f, 120.0f);
            }
        }

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) m_RightMouseNavigating = false;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) m_PanningCamera = false;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !io.KeyAlt) m_OrbitingCamera = false;

        // Fly-look (RMB + WASD/QE)
        if (m_RightMouseNavigating && ctx.editorCamYaw && ctx.editorCamPitch)
        {
            float sens = 0.20f;
            *ctx.editorCamYaw += mouseDelta.x * sens;
            *ctx.editorCamPitch = std::clamp(*ctx.editorCamPitch - mouseDelta.y * sens, -89.0f, 89.0f);

            float yr = glm::radians(*ctx.editorCamYaw);
            float pr = glm::radians(*ctx.editorCamPitch);
            glm::vec3 fwd(std::cos(yr) * std::cos(pr), std::sin(pr), std::sin(yr) * std::cos(pr));
            ctx.editorCamera->forward = glm::normalize(fwd);
            glm::vec3 right = glm::normalize(glm::cross(ctx.editorCamera->forward, glm::vec3(0, 1, 0)));
            glm::vec3 up = glm::normalize(glm::cross(right, ctx.editorCamera->forward));

            float spd = (io.KeyShift ? 28.0f : 12.0f) * dt;
            if (ImGui::IsKeyDown(ImGuiKey_W)) ctx.editorCamera->position += ctx.editorCamera->forward * spd;
            if (ImGui::IsKeyDown(ImGuiKey_S)) ctx.editorCamera->position -= ctx.editorCamera->forward * spd;
            if (ImGui::IsKeyDown(ImGuiKey_D)) ctx.editorCamera->position += right * spd;
            if (ImGui::IsKeyDown(ImGuiKey_A)) ctx.editorCamera->position -= right * spd;
            if (ImGui::IsKeyDown(ImGuiKey_E)) ctx.editorCamera->position += up * spd;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) ctx.editorCamera->position -= up * spd;
        }
        else if (m_OrbitingCamera && ctx.editorCamYaw && ctx.editorCamPitch)
        {
            *ctx.editorCamYaw += mouseDelta.x * 0.35f;
            *ctx.editorCamPitch = std::clamp(*ctx.editorCamPitch - mouseDelta.y * 0.35f, -85.0f, 85.0f);
            float yr = glm::radians(*ctx.editorCamYaw);
            float pr = glm::radians(*ctx.editorCamPitch);
            glm::vec3 fwd = glm::normalize(glm::vec3(std::cos(yr) * std::cos(pr), std::sin(pr), std::sin(yr) * std::cos(pr)));
            ctx.editorCamera->forward = fwd;
            ctx.editorCamera->position = m_OrbitPivot - fwd * m_OrbitDistance;
        }
        else if (m_PanningCamera)
        {
            glm::vec3 right = glm::normalize(glm::cross(ctx.editorCamera->forward, glm::vec3(0, 1, 0)));
            glm::vec3 up = glm::normalize(glm::cross(right, ctx.editorCamera->forward));
            float panScale = 0.025f;
            ctx.editorCamera->position += (-right * mouseDelta.x + up * mouseDelta.y) * panScale;
        }

        // Scroll zoom
        if (vpHovered && std::abs(io.MouseWheel) > 0.001f)
        {
            float zoomStep = io.MouseWheel * (io.KeyShift ? 5.0f : 2.0f);
            ctx.editorCamera->position += ctx.editorCamera->forward * zoomStep;
        }

        // Project ray onto line (gizmoCenter + t * axisVec) parameter t
        auto projectRayOntoAxis = [](const glm::vec3& origin, const glm::vec3& dir, const glm::vec3& axisOrigin, const glm::vec3& axisUnit) -> float {
            glm::vec3 w = origin - axisOrigin;
            float b = glm::dot(dir, axisUnit);
            float d = glm::dot(dir, w);
            float e = glm::dot(axisUnit, w);
            float denom = 1.0f - b * b;
            if (denom < 1e-5f) return 0.0f;
            return (e - b * d) / denom;
        };

        // Gizmo grab or Viewport ray pick on LMB click (when Alt is NOT held)
        if (vpHovered && !io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (m_HoveredGizmoAxis >= 0 && ctx.selectedEntity != kInvalidEntity)
            {
                m_GizmoBeforeSnapshot = ctx.scene->GetEntities();
                m_GizmoBeforeSel = ctx.selectedEntities;
                m_GizmoBeforePrimary = ctx.selectedEntity;
                m_GizmoShiftDuplicated = false;

                // Shift+Drag duplicates selection first!
                if (io.KeyShift && !ctx.selectedEntities.empty())
                {
                    std::vector<EntityID> newIds;
                    for (EntityID id : ctx.selectedEntities)
                    {
                        EntityID dupId = ctx.scene->DuplicateSubtree(id, glm::vec3(0.0f));
                        if (dupId != kInvalidEntity)
                            newIds.push_back(dupId);
                    }
                    if (!newIds.empty())
                    {
                        ctx.selectedEntities = newIds;
                        ctx.selectedEntity = newIds.back();
                        m_GizmoShiftDuplicated = true;
                        ctx.SetToast("Duplicated selection (Shift+Drag)");
                    }
                }

                if (const Entity* primary = ctx.scene->GetEntity(ctx.selectedEntity))
                {
                    m_ActiveGizmoAxis = m_HoveredGizmoAxis;
                    m_GizmoCenter = ctx.scene->GetWorldPosition(*primary);
                    m_GizmoAxisVec = GetGizmoAxisVector(*primary, m_ActiveGizmoAxis, ctx.viewportTools.localSpace, *ctx.scene);
                    m_GizmoDragStartDist = projectRayOntoAxis(rayO, rayD, m_GizmoCenter, m_GizmoAxisVec);

                    // Collect top-level selected entities (skip children whose parent is also selected so movement doesn't double-apply)
                    m_GizmoTargetIds.clear();
                    m_GizmoStartTransforms.clear();
                    m_GizmoStartWorldPositions.clear();
                    for (EntityID id : ctx.selectedEntities)
                    {
                        const Entity* e = ctx.scene->GetEntity(id);
                        if (!e || !e->alive) continue;
                        bool ancestorSelected = false;
                        EntityID p = e->parentId;
                        int guard = 0;
                        while (p != kInvalidEntity && guard < 32)
                        {
                            if (ctx.IsSelected(p)) { ancestorSelected = true; break; }
                            const Entity* pe = ctx.scene->GetEntity(p);
                            p = pe ? pe->parentId : kInvalidEntity;
                            ++guard;
                        }
                        if (!ancestorSelected)
                        {
                            m_GizmoTargetIds.push_back(id);
                            m_GizmoStartTransforms.push_back(e->transform);
                            m_GizmoStartWorldPositions.push_back(ctx.scene->GetWorldPosition(*e));
                        }
                    }
                }
            }
            else
            {
                // Pick entity under cursor
                EntityID hitId = ctx.scene->Raycast(rayO, rayD, nullptr, ctx.MakeBoundsLookup());
                if (io.KeyCtrl)
                {
                    if (hitId != kInvalidEntity)
                        ctx.ToggleSelect(hitId);
                }
                else
                {
                    ctx.SelectSingle(hitId);
                }
            }
        }

        // Active Gizmo Drag
        if (m_ActiveGizmoAxis >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            float curT = projectRayOntoAxis(rayO, rayD, m_GizmoCenter, m_GizmoAxisVec);
            float delta = curT - m_GizmoDragStartDist;

            auto applySnap = [](float v, float step) -> float {
                if (step <= 1e-5f) return v;
                return std::round(v / step) * step;
            };

            for (size_t i = 0; i < m_GizmoTargetIds.size(); ++i)
            {
                Entity* e = ctx.scene->GetEntity(m_GizmoTargetIds[i]);
                if (!e) continue;
                const TransformComponent& startT = m_GizmoStartTransforms[i];

                if (ctx.viewportTools.gizmoMode == GizmoMode::Translate)
                {
                    float moveAmt = ctx.viewportTools.snapEnabled ? applySnap(delta, ctx.viewportTools.moveSnap) : delta;
                    glm::vec3 worldDelta = m_GizmoAxisVec * moveAmt;
                    glm::vec3 targetWorldPos = m_GizmoStartWorldPositions[i] + worldDelta;

                    if (e->parentId != kInvalidEntity)
                    {
                        if (const Entity* p = ctx.scene->GetEntity(e->parentId))
                        {
                            glm::mat4 pWorld = ctx.scene->GetWorldMatrix(*p);
                            if (std::abs(glm::determinant(pWorld)) > 1e-10f)
                                e->transform.position = glm::vec3(glm::inverse(pWorld) * glm::vec4(targetWorldPos, 1.0f));
                            else
                                e->transform.position = targetWorldPos;
                        }
                    }
                    else
                    {
                        e->transform.position = targetWorldPos;
                    }
                }
                else if (ctx.viewportTools.gizmoMode == GizmoMode::Rotate)
                {
                    float rotDeg = delta * 35.0f;
                    if (ctx.viewportTools.snapEnabled)
                        rotDeg = applySnap(rotDeg, ctx.viewportTools.rotateSnapDeg);
                    e->transform.rotation = startT.rotation;
                    e->transform.rotation[m_ActiveGizmoAxis] += rotDeg;
                }
                else if (ctx.viewportTools.gizmoMode == GizmoMode::Scale)
                {
                    float sclDelta = delta;
                    if (ctx.viewportTools.snapEnabled)
                        sclDelta = applySnap(sclDelta, ctx.viewportTools.scaleSnap);
                    e->transform.scale = startT.scale;
                    e->transform.scale[m_ActiveGizmoAxis] = std::max(0.05f, startT.scale[m_ActiveGizmoAxis] + sclDelta);
                }
            }
        }

        // Release Gizmo -> Commit Undo/Redo command
        if (m_ActiveGizmoAxis >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if (ctx.commands)
            {
                if (m_GizmoShiftDuplicated)
                {
                    ctx.commands->RecordSceneSnapshot(
                        *ctx.scene, ctx,
                        std::move(m_GizmoBeforeSnapshot), std::move(m_GizmoBeforeSel), m_GizmoBeforePrimary,
                        ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                        "Duplicate & Move");
                }
                else if (!m_GizmoTargetIds.empty())
                {
                    std::vector<TransformComponent> afterTransforms;
                    afterTransforms.reserve(m_GizmoTargetIds.size());
                    bool changed = false;
                    for (size_t i = 0; i < m_GizmoTargetIds.size(); ++i)
                    {
                        const Entity* e = ctx.scene->GetEntity(m_GizmoTargetIds[i]);
                        TransformComponent cur = e ? e->transform : m_GizmoStartTransforms[i];
                        if (cur.position != m_GizmoStartTransforms[i].position ||
                            cur.rotation != m_GizmoStartTransforms[i].rotation ||
                            cur.scale    != m_GizmoStartTransforms[i].scale)
                        {
                            changed = true;
                        }
                        afterTransforms.push_back(cur);
                    }
                    if (changed)
                    {
                        const char* opName =
                            (ctx.viewportTools.gizmoMode == GizmoMode::Translate) ? "Gizmo Move" :
                            (ctx.viewportTools.gizmoMode == GizmoMode::Rotate) ? "Gizmo Rotate" : "Gizmo Scale";
                        ctx.commands->RecordMultiTransformEdit(
                            *ctx.scene, m_GizmoTargetIds, m_GizmoStartTransforms, afterTransforms, opName);
                    }
                }
            }
            m_ActiveGizmoAxis = -1;
            m_GizmoBeforeSnapshot.clear();
            m_GizmoTargetIds.clear();
            m_GizmoStartTransforms.clear();
            m_GizmoStartWorldPositions.clear();
        }
    }

    void ViewportPanel::DrawViewportOverlay(EditorContext& ctx, const ImVec2& vpMin, const ImVec2& vpSize)
    {
        if (!ctx.viewportTools.showOverlay)
            return;

        const bool isEdit = (!ctx.playState || *ctx.playState == PlayState::Edit);
        const bool isPlaying = (ctx.playState && *ctx.playState == PlayState::Playing);
        const bool wantsCapture = (ctx.gameModule && *ctx.gameModule && (*ctx.gameModule)->WantsCursorCapture());

        // During non-captured active gameplay, avoid cluttering or overlapping the game's HUD
        if (isPlaying && !wantsCapture)
            return;

        if (isEdit)
            ImGui::SetNextWindowPos(ImVec2(vpMin.x + 12.0f, vpMin.y + 10.0f), ImGuiCond_Always);
        else
            ImGui::SetNextWindowPos(ImVec2(vpMin.x + vpSize.x * 0.5f, vpMin.y + vpSize.y - 46.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));

        ImGui::SetNextWindowBgAlpha(0.76f);
        ImGuiWindowFlags ovFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("##ViewportPill", nullptr, ovFlags))
        {
            if (isPlaying)
            {
                if (ctx.gameplayMouseLockActive)
                    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.55f, 1.0f), "PLAYING  |  Mouse captured (ESC releases)");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.30f, 1.0f), "PLAYING  |  Click viewport to capture mouse");
            }
            else if (ctx.playState && *ctx.playState == PlayState::Paused)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "PAUSED  |  Press Step or Resume");
            }
            else
            {
                const char* modeLbl =
                    (ctx.viewportTools.gizmoMode == GizmoMode::Translate) ? "Move [W]" :
                    (ctx.viewportTools.gizmoMode == GizmoMode::Rotate) ? "Rotate [E]" : "Scale [R]";
                const char* spaceLbl = ctx.viewportTools.localSpace ? "Local [X]" : "World [X]";
                if (ctx.viewportTools.showStats && ctx.renderer3D)
                {
                    const auto& st = ctx.renderer3D->GetStats();
                    ImGui::Text("%s  |  %s  |  Snap: %s  |  %ux%u  |  %u draws / %u tris",
                        modeLbl, spaceLbl,
                        ctx.viewportTools.snapEnabled ? "ON" : "OFF",
                        static_cast<unsigned>(vpSize.x), static_cast<unsigned>(vpSize.y),
                        st.drawCalls, st.triangleCount);
                }
                else
                {
                    ImGui::Text("%s  |  %s  |  Snap: %s",
                        modeLbl, spaceLbl, ctx.viewportTools.snapEnabled ? "ON" : "OFF");
                }
            }
        }
        ImGui::End();
    }
}
