#include "HierarchyPanel.h"
#include "../commands/EditorCommandSystem.h"
#include "../../engine/scene/SceneSerializer.h"
#include "../../engine/core/Log.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

namespace Hargon
{
    namespace
    {
        bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle)
        {
            if (needle.empty()) return true;
            auto it = std::search(
                haystack.begin(), haystack.end(),
                needle.begin(), needle.end(),
                [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                });
            return it != haystack.end();
        }

        bool EntityOrDescendantMatchesFilter(const Scene& scene, EntityID id, const std::string& filter, std::unordered_set<EntityID>& visited)
        {
            if (visited.count(id)) return false;
            visited.insert(id);

            const Entity* e = scene.GetEntity(id);
            if (!e || !e->alive) return false;
            if (ContainsCaseInsensitive(e->tag.name, filter) || ContainsCaseInsensitive(e->tag.tag, filter))
                return true;

            for (const auto& child : scene.GetEntities())
            {
                if (child.alive && child.parentId == id)
                {
                    if (EntityOrDescendantMatchesFilter(scene, child.id, filter, visited))
                        return true;
                }
            }
            return false;
        }
    }

    void HierarchyPanel::Draw(EditorContext& ctx)
    {
        if (!ctx.showHierarchy)
            return;

        if (!ImGui::Begin(kWinHierarchy, &ctx.showHierarchy))
        {
            ImGui::End();
            return;
        }

        if (!ctx.scene)
        {
            ImGui::TextDisabled("No active scene loaded.");
            ImGui::End();
            return;
        }

        // Handle pending F2 rename request from global shortcuts
        if (ctx.renameRequestId != kInvalidEntity)
        {
            if (const Entity* renEnt = ctx.scene->GetEntity(ctx.renameRequestId))
            {
                m_RenameEntityId = renEnt->id;
                std::snprintf(m_RenameBuf, sizeof(m_RenameBuf), "%s", renEnt->tag.name.c_str());
                m_OpenRenamePopup = true;
            }
            ctx.renameRequestId = kInvalidEntity;
        }

        // Top controls: Quick Create + Filter Search
        if (ImGui::Button("+ Add"))
            ImGui::OpenPopup("HierarchyCreatePopup");
        if (ImGui::BeginPopup("HierarchyCreatePopup"))
        {
            DrawCreateMenuItems(ctx, ctx.selectedEntity);
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##HierarchyFilter", "Filter nodes or tags...", m_SearchBuf, sizeof(m_SearchBuf));

        ImGui::Separator();

        // Root Drop Target bar (visible when dragging an entity)
        if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
        {
            if (std::string(payload->DataType) == "HARGON_ENTITY")
            {
                ImGui::Button("Drop here to move to Scene Root", ImVec2(-1.0f, 24.0f));
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* accept = ImGui::AcceptDragDropPayload("HARGON_ENTITY"))
                    {
                        EntityID draggedId = *static_cast<const EntityID*>(accept->Data);
                        if (Entity* e = ctx.scene->GetEntity(draggedId))
                        {
                            if (e->parentId != kInvalidEntity && ctx.commands)
                            {
                                auto beforeEnts = ctx.scene->GetEntities();
                                auto beforeSel = ctx.selectedEntities;
                                EntityID beforePrimary = ctx.selectedEntity;
                                if (ctx.scene->SetParentKeepWorld(draggedId, kInvalidEntity))
                                {
                                    ctx.commands->RecordSceneSnapshot(
                                        *ctx.scene, ctx,
                                        std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                                        ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                                        "Unparent to Root");
                                    ctx.SetToast("Moved to Scene Root");
                                }
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        }

        std::vector<EntityID> visibleOrder;
        ImGui::BeginChild("HierarchyTreeScroll", ImVec2(0.0f, 0.0f), false);

        const std::vector<Entity>& ents = ctx.scene->GetEntities();
        for (const auto& e : ents)
        {
            if (!e.alive) continue;
            bool isRoot = (e.parentId == kInvalidEntity) || (ctx.scene->GetEntity(e.parentId) == nullptr) || (e.parentId == e.id);
            if (isRoot)
                DrawNodeRecursive(ctx, e.id, visibleOrder, 0);
        }

        // Empty-space context menu
        if (ImGui::BeginPopupContextWindow("HierarchyEmptyCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawCreateMenuItems(ctx, kInvalidEntity);
            ImGui::EndPopup();
        }

        // Accept Prefab / OBJ drops from Asset Browser onto Hierarchy root
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* accept = ImGui::AcceptDragDropPayload("HARGON_ASSET"))
            {
                std::string relPath(static_cast<const char*>(accept->Data));
                if (ctx.instantiateAssetInScene)
                {
                    glm::vec3 spawnPos = ctx.editorCamera ? (ctx.editorCamera->position + ctx.editorCamera->forward * 7.0f) : glm::vec3(0.0f);
                    ctx.instantiateAssetInScene(relPath, &spawnPos);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::EndChild();
        ExecuteRenameModal(ctx);
        ImGui::End();
    }

    void HierarchyPanel::DrawNodeRecursive(
        EditorContext& ctx,
        EntityID entityId,
        std::vector<EntityID>& visibleOrder,
        int depth)
    {
        if (depth > 32) return;
        const Entity* e = ctx.scene->GetEntity(entityId);
        if (!e || !e->alive) return;

        std::string filterStr(m_SearchBuf);
        if (!filterStr.empty())
        {
            std::unordered_set<EntityID> vis;
            if (!EntityOrDescendantMatchesFilter(*ctx.scene, entityId, filterStr, vis))
                return;
        }

        visibleOrder.push_back(entityId);

        // Find live children in current entity order
        std::vector<EntityID> childIds;
        for (const auto& cand : ctx.scene->GetEntities())
        {
            if (cand.alive && cand.parentId == entityId && cand.id != entityId)
                childIds.push_back(cand.id);
        }

        const bool hasChildren = !childIds.empty();
        const bool isSelected = ctx.IsSelected(entityId);

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;
        if (depth == 0 || !filterStr.empty())
            flags |= ImGuiTreeNodeFlags_DefaultOpen;

        const char* badge = "[N]";
        ImVec4 badgeColor(0.65f, 0.72f, 0.80f, 1.0f);
        if (e->HasLight())           { badge = "[L]"; badgeColor = ImVec4(1.0f, 0.85f, 0.35f, 1.0f); }
        else if (e->HasCamera())     { badge = "[C]"; badgeColor = ImVec4(0.45f, 0.88f, 1.0f, 1.0f); }
        else if (e->HasParticles())  { badge = "[P]"; badgeColor = ImVec4(0.95f, 0.55f, 0.90f, 1.0f); }
        else if (e->HasLuaScript())  { badge = "[S]"; badgeColor = ImVec4(0.45f, 0.92f, 0.55f, 1.0f); }
        else if (e->HasMesh())       { badge = "[M]"; badgeColor = ImVec4(0.70f, 0.84f, 1.0f, 1.0f); }

        ImGui::PushID(static_cast<int>(entityId));
        if (e->processMode == ProcessMode::Disabled)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.54f, 0.60f, 1.0f));

        char labelBuf[256];
        std::snprintf(labelBuf, sizeof(labelBuf), "%s %s", badge, e->tag.name.c_str());
        bool opened = ImGui::TreeNodeEx("##node", flags, "%s", labelBuf);

        if (e->processMode == ProcessMode::Disabled)
            ImGui::PopStyleColor();

        // Subtle role color indicator on hover tooltip
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s\nTag: %s | ID: %u", e->tag.name.c_str(), e->tag.tag.c_str(), entityId);

        // Selection handling (Single, Ctrl+Toggle, Shift+Range)
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl)
            {
                ctx.ToggleSelect(entityId);
                m_LastClickedId = entityId;
            }
            else if (io.KeyShift && m_LastClickedId != kInvalidEntity)
            {
                auto itA = std::find(visibleOrder.begin(), visibleOrder.end(), m_LastClickedId);
                auto itB = std::find(visibleOrder.begin(), visibleOrder.end(), entityId);
                if (itA != visibleOrder.end() && itB != visibleOrder.end())
                {
                    if (itA > itB) std::swap(itA, itB);
                    ctx.ClearSelection();
                    for (auto it = itA; it <= itB; ++it)
                        ctx.AddToSelection(*it);
                    ctx.selectedEntity = entityId;
                }
                else
                {
                    ctx.SelectSingle(entityId);
                }
            }
            else
            {
                ctx.SelectSingle(entityId);
                m_LastClickedId = entityId;
            }
        }

        // Drag source
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::SetDragDropPayload("HARGON_ENTITY", &entityId, sizeof(EntityID));
            ImGui::TextColored(badgeColor, "%s %s", badge, e->tag.name.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target: top 25% = reorder before, bottom 25% = reorder after, middle = reparent inside
        if (ImGui::BeginDragDropTarget())
        {
            ImVec2 itemMin = ImGui::GetItemRectMin();
            ImVec2 itemMax = ImGui::GetItemRectMax();
            float h = std::max(1.0f, itemMax.y - itemMin.y);
            float relY = (ImGui::GetMousePos().y - itemMin.y) / h;

            int dropZone = (relY < 0.25f) ? -1 : ((relY > 0.75f) ? 1 : 0);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (dropZone == -1)
                dl->AddLine(ImVec2(itemMin.x, itemMin.y), ImVec2(itemMax.x, itemMin.y), IM_COL32(80, 210, 255, 255), 2.5f);
            else if (dropZone == 1)
                dl->AddLine(ImVec2(itemMin.x, itemMax.y), ImVec2(itemMax.x, itemMax.y), IM_COL32(80, 210, 255, 255), 2.5f);
            else
                dl->AddRect(itemMin, itemMax, IM_COL32(80, 210, 255, 230), 2.0f, 0, 2.0f);

            if (const ImGuiPayload* accept = ImGui::AcceptDragDropPayload("HARGON_ENTITY"))
            {
                EntityID draggedId = *static_cast<const EntityID*>(accept->Data);
                if (draggedId != entityId && ctx.commands)
                {
                    auto beforeEnts = ctx.scene->GetEntities();
                    auto beforeSel = ctx.selectedEntities;
                    EntityID beforePrimary = ctx.selectedEntity;

                    bool mutated = false;
                    if (dropZone == 0)
                    {
                        mutated = ctx.scene->SetParentKeepWorld(draggedId, entityId);
                        if (mutated) ctx.SetToast("Reparented entity");
                    }
                    else
                    {
                        const Entity* targetEnt = ctx.scene->GetEntity(entityId);
                        EntityID targetParent = targetEnt ? targetEnt->parentId : kInvalidEntity;
                        if (ctx.scene->CanReparent(draggedId, targetParent))
                        {
                            const Entity* draggedEnt = ctx.scene->GetEntity(draggedId);
                            if (draggedEnt && draggedEnt->parentId != targetParent)
                                ctx.scene->SetParentKeepWorld(draggedId, targetParent);
                            mutated = ctx.scene->ReorderEntity(draggedId, entityId, dropZone == 1) || (draggedEnt != nullptr);
                            if (mutated) ctx.SetToast("Reordered entity");
                        }
                    }

                    if (mutated)
                    {
                        ctx.commands->RecordSceneSnapshot(
                            *ctx.scene, ctx,
                            std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                            ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                            dropZone == 0 ? "Reparent Entity" : "Reorder Entity");
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Context Menu
        if (ImGui::BeginPopupContextItem("EntityNodeCtx"))
        {
            if (!ctx.IsSelected(entityId))
                ctx.SelectSingle(entityId);
            DrawEntityContextMenu(ctx, entityId);
            ImGui::EndPopup();
        }

        if (hasChildren && opened)
        {
            for (EntityID cid : childIds)
                DrawNodeRecursive(ctx, cid, visibleOrder, depth + 1);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void HierarchyPanel::DrawCreateMenuItems(EditorContext& ctx, EntityID parentId)
    {
        auto createNode = [&](const char* name, const char* tag, MeshPrimitive prim, bool hasMesh, bool hasLight, bool hasCam, bool hasPart) {
            if (!ctx.scene || !ctx.commands) return;
            auto beforeEnts = ctx.scene->GetEntities();
            auto beforeSel = ctx.selectedEntities;
            EntityID beforePrimary = ctx.selectedEntity;

            Entity& e = ctx.scene->CreateEntity(name, tag);
            e.hasMesh = hasMesh;
            e.mesh.enabled = hasMesh;
            e.mesh.primitive = prim;
            e.hasMaterial = hasMesh;

            if (hasLight)
            {
                e.hasLight = true;
                e.light.enabled = true;
                e.light.color = glm::vec3(0.4f, 0.85f, 1.0f);
                e.light.intensity = 3.5f;
                e.light.range = 14.0f;
                e.transform.scale = glm::vec3(0.35f);
            }
            if (hasCam)
            {
                e.hasCamera = true;
                e.camera.enabled = true;
                e.camera.mode = CameraMode::ThirdPersonOrbit;
            }
            if (hasPart)
            {
                e.hasParticles = true;
                e.particleEmitter.enabled = true;
                e.particleEmitter.emitting = true;
                e.particleEmitter.emissionShape = 3;
                e.particleEmitter.emissionRate = 25.0f;
                e.transform.scale = glm::vec3(0.45f);
            }

            if (parentId != kInvalidEntity && ctx.scene->GetEntity(parentId))
            {
                ctx.scene->SetParent(e.id, parentId);
                e.transform.position = glm::vec3(0.0f, 1.5f, 0.0f);
            }
            else if (ctx.editorCamera)
            {
                e.transform.position = ctx.editorCamera->position + ctx.editorCamera->forward * 7.5f;
                e.transform.position.y = std::max(0.5f, e.transform.position.y);
            }

            ctx.SelectSingle(e.id);
            ctx.commands->RecordSceneSnapshot(
                *ctx.scene, ctx,
                std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                std::string("Create ") + name);
            ctx.SetToast(std::string("Created ") + name);
        };

        if (ImGui::MenuItem("Empty Node3D"))  createNode("Node3D", "Default", MeshPrimitive::Cube, false, false, false, false);
        if (ImGui::MenuItem("Cube"))          createNode("Cube", "Default", MeshPrimitive::Cube, true, false, false, false);
        if (ImGui::MenuItem("Sphere"))        createNode("Sphere", "Default", MeshPrimitive::Sphere, true, false, false, false);
        if (ImGui::MenuItem("Cylinder"))      createNode("Cylinder", "Default", MeshPrimitive::Cylinder, true, false, false, false);
        if (ImGui::MenuItem("Cone"))          createNode("Cone", "Default", MeshPrimitive::Cone, true, false, false, false);
        if (ImGui::MenuItem("Torus"))         createNode("Torus", "Default", MeshPrimitive::Torus, true, false, false, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Point Light"))   createNode("PointLight", "Light", MeshPrimitive::Sphere, true, true, false, false);
        if (ImGui::MenuItem("Camera3D"))      createNode("Camera3D", "Camera", MeshPrimitive::Cube, false, false, true, false);
        if (ImGui::MenuItem("Particle Fountain")) createNode("ParticleFountain", "Default", MeshPrimitive::Sphere, true, false, false, true);
    }

    void HierarchyPanel::DrawEntityContextMenu(EditorContext& ctx, EntityID targetId)
    {
        if (ImGui::BeginMenu("Create Child"))
        {
            DrawCreateMenuItems(ctx, targetId);
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
        {
            auto beforeEnts = ctx.scene->GetEntities();
            auto beforeSel = ctx.selectedEntities;
            EntityID beforePrimary = ctx.selectedEntity;

            std::vector<EntityID> toDup = ctx.selectedEntities.empty()
                ? std::vector<EntityID>{ targetId }
                : ctx.selectedEntities;
            std::vector<EntityID> newSel;
            for (EntityID id : toDup)
            {
                EntityID dupId = ctx.scene->DuplicateSubtree(id, glm::vec3(1.5f, 0.0f, 1.5f));
                if (dupId != kInvalidEntity)
                    newSel.push_back(dupId);
            }
            if (!newSel.empty())
            {
                ctx.selectedEntities = newSel;
                ctx.selectedEntity = newSel.back();
                ctx.commands->RecordSceneSnapshot(
                    *ctx.scene, ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                    "Duplicate");
            }
        }

        if (ImGui::MenuItem("Rename", "F2"))
        {
            if (const Entity* e = ctx.scene->GetEntity(targetId))
            {
                m_RenameEntityId = targetId;
                std::snprintf(m_RenameBuf, sizeof(m_RenameBuf), "%s", e->tag.name.c_str());
                m_OpenRenamePopup = true;
            }
        }

        if (ImGui::MenuItem("Group Selected"))
        {
            std::vector<EntityID> ids = ctx.selectedEntities.empty()
                ? std::vector<EntityID>{ targetId }
                : ctx.selectedEntities;
            auto beforeEnts = ctx.scene->GetEntities();
            auto beforeSel = ctx.selectedEntities;
            EntityID beforePrimary = ctx.selectedEntity;

            EntityID grp = ctx.scene->GroupEntities(ids, "Group");
            if (grp != kInvalidEntity)
            {
                ctx.SelectSingle(grp);
                ctx.commands->RecordSceneSnapshot(
                    *ctx.scene, ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                    "Group Entities");
                ctx.SetToast("Grouped selected entities");
            }
        }

        const Entity* targetEnt = ctx.scene->GetEntity(targetId);
        if (targetEnt && targetEnt->parentId != kInvalidEntity)
        {
            if (ImGui::MenuItem("Clear Parent (Move to Root)"))
            {
                auto beforeEnts = ctx.scene->GetEntities();
                auto beforeSel = ctx.selectedEntities;
                EntityID beforePrimary = ctx.selectedEntity;
                if (ctx.scene->SetParentKeepWorld(targetId, kInvalidEntity))
                {
                    ctx.commands->RecordSceneSnapshot(
                        *ctx.scene, ctx,
                        std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                        ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                        "Unparent Entity");
                }
            }
        }

        if (ImGui::MenuItem("Drop to Surface", "End"))
        {
            auto beforeEnts = ctx.scene->GetEntities();
            auto beforeSel = ctx.selectedEntities;
            EntityID beforePrimary = ctx.selectedEntity;
            auto lookup = ctx.MakeBoundsLookup();
            bool anyMoved = false;
            std::vector<EntityID> ids = ctx.selectedEntities.empty() ? std::vector<EntityID>{ targetId } : ctx.selectedEntities;
            for (EntityID id : ids)
                anyMoved |= ctx.scene->DropEntityToSurface(id, lookup);
            if (anyMoved)
            {
                ctx.commands->RecordSceneSnapshot(
                    *ctx.scene, ctx,
                    std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                    ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                    "Drop to Surface");
            }
            else
            {
                ctx.SetToast("No surface below selected entity");
            }
        }

        if (ImGui::MenuItem("Save as Prefab (.hprefab)..."))
        {
            if (ctx.projectConfig && targetEnt)
            {
                fs::path prefabDir = fs::path(ctx.projectConfig->projectRootDir) / "prefabs";
                std::error_code ec;
                fs::create_directories(prefabDir, ec);
                std::string safeName;
                for (char ch : targetEnt->tag.name)
                {
                    if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')
                        safeName += ch;
                }
                if (safeName.empty()) safeName = "prefab";
                fs::path prefabFile = prefabDir / (safeName + ".hprefab");
                if (SceneSerializer::SaveEntityPrefab(*targetEnt, prefabFile.string()))
                    ctx.SetToast("Saved prefab: " + prefabFile.filename().string());
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Delete", "Del"))
        {
            auto beforeEnts = ctx.scene->GetEntities();
            auto beforeSel = ctx.selectedEntities;
            EntityID beforePrimary = ctx.selectedEntity;

            std::vector<EntityID> toDel = ctx.selectedEntities.empty()
                ? std::vector<EntityID>{ targetId }
                : ctx.selectedEntities;
            for (EntityID id : toDel)
            {
                if (ctx.scene->GetEntity(id))
                    ctx.scene->DestroySubtree(id);
            }
            ctx.ClearSelection();
            ctx.commands->RecordSceneSnapshot(
                *ctx.scene, ctx,
                std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                "Delete Entity");
        }
    }

    void HierarchyPanel::ExecuteRenameModal(EditorContext& ctx)
    {
        if (m_OpenRenamePopup)
        {
            ImGui::OpenPopup("Rename Entity");
            m_OpenRenamePopup = false;
        }
        if (ImGui::BeginPopupModal("Rename Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            bool enterPressed = ImGui::InputText("New Name", m_RenameBuf, sizeof(m_RenameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            if (enterPressed || ImGui::Button("Apply", ImVec2(100, 0)))
            {
                if (Entity* e = ctx.scene->GetEntity(m_RenameEntityId))
                {
                    std::string trimmed(m_RenameBuf);
                    if (!trimmed.empty() && trimmed != e->tag.name)
                    {
                        Entity before = *e;
                        e->tag.name = trimmed;
                        ctx.commands->RecordEntityEdit(*ctx.scene, e->id, before, *e, "Rename Entity");
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
}
