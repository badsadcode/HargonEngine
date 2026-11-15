#include "InspectorPanel.h"
#include "../commands/EditorCommandSystem.h"
#include "../../engine/project/ProjectManager.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace Hargon
{
    namespace
    {
        bool ContainsNoCase(const std::string& hay, const std::string& needle)
        {
            if (needle.empty()) return true;
            auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(),
                [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                });
            return it != hay.end();
        }
    }

    bool InspectorPanel::MatchesFilter(const char* sectionName, const char* keywords) const
    {
        std::string q(m_PropSearchBuf);
        if (q.empty()) return true;
        return ContainsNoCase(sectionName ? sectionName : "", q) || ContainsNoCase(keywords ? keywords : "", q);
    }

    void InspectorPanel::CommitWidgetEdit(
        EditorContext& ctx,
        Entity* ent,
        const Entity& preWidgetState,
        bool edited,
        const std::string& label,
        const std::string& mergeKey)
    {
        if (!ent || !ctx.scene || !ctx.commands)
            return;

        if (ImGui::IsItemActivated())
        {
            m_ActiveMergeKey = mergeKey;
            m_DragStartEntity = preWidgetState;
            m_HasDragStart = true;
        }
        if (edited)
        {
            const Entity& baseBefore = (m_HasDragStart && m_ActiveMergeKey == mergeKey)
                ? m_DragStartEntity
                : preWidgetState;
            ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, baseBefore, *ent, label, mergeKey);
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            if (m_ActiveMergeKey == mergeKey)
                m_HasDragStart = false;
        }
    }

    void InspectorPanel::CommitEnvWidgetEdit(
        EditorContext& ctx,
        const SceneEnvSnapshot& preState,
        bool edited,
        const std::string& label,
        const std::string& mergeKey)
    {
        if (!ctx.scene || !ctx.commands)
            return;

        if (ImGui::IsItemActivated())
        {
            m_EnvMergeKey = mergeKey;
            m_EnvDragStart = preState;
            m_HasEnvDragStart = true;
        }
        if (edited)
        {
            const SceneEnvSnapshot& baseBefore = (m_HasEnvDragStart && m_EnvMergeKey == mergeKey)
                ? m_EnvDragStart
                : preState;
            ctx.commands->RecordEnvironmentEdit(*ctx.scene, baseBefore, SceneEnvSnapshot::Capture(*ctx.scene), label, mergeKey);
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            if (m_EnvMergeKey == mergeKey)
                m_HasEnvDragStart = false;
        }
    }

    void InspectorPanel::Draw(EditorContext& ctx)
    {
        if (!ctx.showInspector)
            return;

        if (!ImGui::Begin(kWinInspector, &ctx.showInspector))
        {
            ImGui::End();
            return;
        }

        if (!ctx.scene)
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        ctx.PruneInvalidSelection();
        Entity* ent = (ctx.selectedEntity != kInvalidEntity) ? ctx.scene->GetEntity(ctx.selectedEntity) : nullptr;
        if (!ent || !ent->alive)
        {
            ImGui::Spacing();
            ImGui::TextWrapped("Select an object in the 3D Viewport or Scene Hierarchy to inspect and edit its components.");
            ImGui::Spacing();
            if (ImGui::Button("Open World & Environment"))
                ctx.showWorldEnv = true;
            ImGui::End();
            return;
        }

        // Multi-selection quick action ribbon
        if (ctx.selectedEntities.size() > 1)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.17f, 0.26f, 0.90f));
            ImGui::BeginChild("MultiSelBanner", ImVec2(0.0f, 54.0f), true);
            ImGui::TextColored(ImVec4(0.42f, 0.88f, 1.0f, 1.0f), "%zu entities selected (Inspecting: %s)",
                ctx.selectedEntities.size(), ent->tag.name.c_str());
            if (ImGui::SmallButton("Group Selected"))
            {
                auto beforeEnts = ctx.scene->GetEntities();
                auto beforeSel = ctx.selectedEntities;
                EntityID beforePrimary = ctx.selectedEntity;
                EntityID grp = ctx.scene->GroupEntities(ctx.selectedEntities, "Group");
                if (grp != kInvalidEntity)
                {
                    ctx.SelectSingle(grp);
                    ctx.commands->RecordSceneSnapshot(*ctx.scene, ctx,
                        std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                        ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                        "Group Selected");
                    ctx.SetToast("Grouped selection");
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Drop All to Surface"))
            {
                auto beforeEnts = ctx.scene->GetEntities();
                auto beforeSel = ctx.selectedEntities;
                EntityID beforePrimary = ctx.selectedEntity;
                auto lookup = ctx.MakeBoundsLookup();
                bool anyMoved = false;
                for (EntityID id : ctx.selectedEntities)
                    anyMoved |= ctx.scene->DropEntityToSurface(id, lookup);
                if (anyMoved)
                {
                    ctx.commands->RecordSceneSnapshot(*ctx.scene, ctx,
                        std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                        ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                        "Drop Selected to Surface");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Entity Identity & Parenting Header
        {
            Entity pre = *ent;
            char nameBuf[128];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", ent->tag.name.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            bool nameEdited = ImGui::InputText("##EntName", nameBuf, sizeof(nameBuf));
            if (nameEdited) ent->tag.name = nameBuf;
            CommitWidgetEdit(ctx, ent, pre, nameEdited, "Rename Entity", "ent_name_" + std::to_string(ent->id));
        }

        // Tag + Quick Preset + Parent Picker + Process Mode
        {
            Entity pre = *ent;
            char tagBuf[64];
            std::snprintf(tagBuf, sizeof(tagBuf), "%s", ent->tag.tag.c_str());
            ImGui::SetNextItemWidth(140.0f);
            bool tagEdited = ImGui::InputText("Tag", tagBuf, sizeof(tagBuf));
            if (tagEdited) ent->tag.tag = tagBuf;
            CommitWidgetEdit(ctx, ent, pre, tagEdited, "Change Gameplay Tag", "ent_tag_" + std::to_string(ent->id));

            ImGui::SameLine();
            if (ImGui::SmallButton("Presets##Tag"))
                ImGui::OpenPopup("TagPresetPopup");
            if (ImGui::BeginPopup("TagPresetPopup"))
            {
                static const char* kPresets[] = { "Default", "Player", "Enemy", "Environment", "Pickup", "Interactable", "Trigger", "Camera", "Light", "Group" };
                for (const char* p : kPresets)
                {
                    if (ImGui::MenuItem(p))
                    {
                        Entity beforeTag = *ent;
                        ent->tag.tag = p;
                        ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, beforeTag, *ent, "Set Tag Preset");
                    }
                }
                ImGui::EndPopup();
            }
        }

        // Parent Name Picker (no integer Parent Node ID)
        {
            std::string parentLabel = "(Scene Root)";
            if (ent->parentId != kInvalidEntity)
            {
                if (const Entity* p = ctx.scene->GetEntity(ent->parentId))
                    parentLabel = p->tag.name;
                else
                    parentLabel = "(Missing Parent)";
            }

            if (ImGui::BeginCombo("Parent", parentLabel.c_str()))
            {
                if (ImGui::Selectable("(Scene Root)", ent->parentId == kInvalidEntity))
                {
                    if (ent->parentId != kInvalidEntity && ctx.commands)
                    {
                        auto beforeEnts = ctx.scene->GetEntities();
                        auto beforeSel = ctx.selectedEntities;
                        EntityID beforePrimary = ctx.selectedEntity;
                        if (ctx.scene->SetParentKeepWorld(ent->id, kInvalidEntity))
                        {
                            ctx.commands->RecordSceneSnapshot(*ctx.scene, ctx,
                                std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                                ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                                "Unparent Entity");
                        }
                    }
                }
                for (const auto& cand : ctx.scene->GetEntities())
                {
                    if (!cand.alive || cand.id == ent->id) continue;
                    if (!ctx.scene->CanReparent(ent->id, cand.id)) continue;

                    char itemBuf[160];
                    std::snprintf(itemBuf, sizeof(itemBuf), "%s##par_%u", cand.tag.name.c_str(), cand.id);
                    bool selected = (ent->parentId == cand.id);
                    if (ImGui::Selectable(itemBuf, selected))
                    {
                        auto beforeEnts = ctx.scene->GetEntities();
                        auto beforeSel = ctx.selectedEntities;
                        EntityID beforePrimary = ctx.selectedEntity;
                        if (ctx.scene->SetParentKeepWorld(ent->id, cand.id))
                        {
                            ctx.commands->RecordSceneSnapshot(*ctx.scene, ctx,
                                std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                                ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                                "Reparent Entity");
                        }
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("Changes parent while keeping world position, rotation, and scale intact.");
        }

        // Process Mode + Groups
        {
            Entity pre = *ent;
            const char* procModes[] = { "Inherit", "Pausable", "When Paused", "Always", "Disabled" };
            int pm = static_cast<int>(ent->processMode);
            if (ImGui::Combo("Process Mode", &pm, procModes, IM_ARRAYSIZE(procModes)))
            {
                ent->processMode = static_cast<ProcessMode>(pm);
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Change Process Mode");
            }
        }

        // Groups compact chips
        if (ImGui::TreeNodeEx("Node Groups", ImGuiTreeNodeFlags_None))
        {
            for (size_t i = 0; i < ent->tag.groups.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                ImGui::SmallButton(ent->tag.groups[i].c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("x"))
                {
                    Entity beforeGrp = *ent;
                    ent->tag.groups.erase(ent->tag.groups.begin() + static_cast<ptrdiff_t>(i));
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, beforeGrp, *ent, "Remove Node Group");
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputTextWithHint("##NewGroup", "Add group...", m_NewGroupBuf, sizeof(m_NewGroupBuf));
            ImGui::SameLine();
            if (ImGui::Button("Add##Grp") && m_NewGroupBuf[0] != '\0')
            {
                if (!ent->tag.IsInGroup(m_NewGroupBuf))
                {
                    Entity beforeGrp = *ent;
                    ent->tag.groups.push_back(m_NewGroupBuf);
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, beforeGrp, *ent, "Add Node Group");
                }
                m_NewGroupBuf[0] = '\0';
            }
            ImGui::TreePop();
        }

        ImGui::Separator();

        // Property Search Bar + Add Component Menu
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 135.0f);
        ImGui::InputTextWithHint("##PropSearch", "Search properties...", m_PropSearchBuf, sizeof(m_PropSearchBuf));
        ImGui::SameLine();
        if (ImGui::Button("+ Add Component", ImVec2(128.0f, 0.0f)))
            ImGui::OpenPopup("AddComponentPopup");

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            auto addComp = [&](const char* label, bool alreadyHas, auto enablerFn) {
                if (alreadyHas) ImGui::BeginDisabled();
                if (ImGui::MenuItem(label))
                {
                    Entity beforeAdd = *ent;
                    enablerFn(*ent);
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, beforeAdd, *ent, std::string("Add ") + label);
                }
                if (alreadyHas) ImGui::EndDisabled();
            };

            addComp("Mesh", ent->HasMesh(), [](Entity& e) { e.hasMesh = true; e.mesh.enabled = true; });
            addComp("Material", ent->HasMaterial(), [](Entity& e) { e.hasMaterial = true; });
            addComp("Light", ent->HasLight(), [](Entity& e) { e.hasLight = true; e.light.enabled = true; });
            addComp("Camera3D", ent->HasCamera(), [](Entity& e) { e.hasCamera = true; e.camera.enabled = true; });
            addComp("RigidBody3D", ent->HasRigidBody(), [](Entity& e) { e.hasRigidBody = true; e.rigidBody.enabled = true; });
            addComp("Collider3D", ent->HasCollider(), [](Entity& e) { e.hasCollider = true; e.collider.enabled = true; });
            addComp("RayCast3D", ent->HasRayCast(), [](Entity& e) { e.hasRayCast = true; e.raycast.enabled = true; });
            addComp("AudioSource3D", ent->HasAudio(), [](Entity& e) { e.hasAudio = true; e.audioSource.enabled = true; });
            addComp("Lua Script", ent->HasLuaScript(), [](Entity& e) { e.hasLuaScript = true; e.luaScript.enabled = true; });
            addComp("CPUParticles3D", ent->HasParticles(), [](Entity& e) { e.hasParticles = true; e.particleEmitter.enabled = true; e.particleEmitter.emitting = true; });
            addComp("AnimationPlayer", ent->HasAnimation(), [](Entity& e) { e.hasAnimation = true; e.animation.enabled = true; });
            ImGui::EndPopup();
        }

        ImGui::Separator();

        // Scrollable Components Area (only showing attached components)
        ImGui::BeginChild("InspectorScroll", ImVec2(0.0f, 0.0f), false);
        ImGui::PushItemWidth(std::max(100.0f, ImGui::GetContentRegionAvail().x * 0.54f));

        if (MatchesFilter("Transform", "position rotation scale world local"))
            DrawTransformCard(ctx, ent);
        if (ent->HasMesh() && MatchesFilter("Mesh", "primitive obj shadows"))
            DrawMeshCard(ctx, ent);
        if (ent->HasMaterial() && MatchesFilter("Material", "albedo color metallic roughness uv pattern emissive"))
            DrawMaterialCard(ctx, ent);
        if (ent->HasLight() && MatchesFilter("Light", "directional point color intensity range shadows"))
            DrawLightCard(ctx, ent);
        if (ent->HasCamera() && MatchesFilter("Camera", "fov near far spring orbit active"))
            DrawCameraCard(ctx, ent);
        if (ent->HasRigidBody() && MatchesFilter("RigidBody", "mass gravity velocity drag bounce"))
            DrawRigidBodyCard(ctx, ent);
        if (ent->HasCollider() && MatchesFilter("Collider", "shape box sphere capsule extents radius trigger layer mask"))
            DrawColliderCard(ctx, ent);
        if (ent->HasRayCast() && MatchesFilter("RayCast3D", "target mask hit"))
            DrawRayCastCard(ctx, ent);
        if (ent->HasAudio() && MatchesFilter("AudioSource3D", "sound volume pitch loop 3d distance"))
            DrawAudioCard(ctx, ent);
        if (ent->HasLuaScript() && MatchesFilter("Lua Script", "lua path script"))
            DrawLuaScriptCard(ctx, ent);
        if (ent->HasParticles() && MatchesFilter("CPUParticles3D", "emitting rate shape speed lifetime color"))
            DrawParticlesCard(ctx, ent);
        if (ent->HasAnimation() && MatchesFilter("AnimationPlayer", "clip speed loop play"))
            DrawAnimationCard(ctx, ent);
        if ((ent->tag.tag == "Player" || ent->tag.tag == "Enemy" || ent->tag.tag == "Pickup" || ent->gameplay.maxHealth != 100.0f || ent->gameplay.scoreValue != 0) &&
            MatchesFilter("Gameplay Attributes", "health shield score damage speed team"))
            DrawGameplayCard(ctx, ent);

        ImGui::PopItemWidth();
        ImGui::EndChild();
        ImGui::End();
    }

    void InspectorPanel::DrawTransformCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::SmallButton("Copy##Tr")) { ctx.clipboard.type = ComponentClipboard::Type::Transform; ctx.clipboard.transform = ent->transform; ctx.SetToast("Copied Transform"); }
            ImGui::SameLine();
            if (ctx.clipboard.type == ComponentClipboard::Type::Transform && ImGui::SmallButton("Paste##Tr"))
            {
                Entity pre = *ent;
                ent->transform = ctx.clipboard.transform;
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Paste Transform");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Drop to Surface"))
            {
                auto beforeEnts = ctx.scene->GetEntities();
                auto beforeSel = ctx.selectedEntities;
                EntityID beforePrimary = ctx.selectedEntity;
                if (ctx.scene->DropEntityToSurface(ent->id, ctx.MakeBoundsLookup()))
                    ctx.commands->RecordSceneSnapshot(*ctx.scene, ctx,
                        std::move(beforeEnts), std::move(beforeSel), beforePrimary,
                        ctx.scene->GetEntities(), ctx.selectedEntities, ctx.selectedEntity,
                        "Drop to Surface");
                else
                    ctx.SetToast("No surface found below entity");
            }

            {
                Entity pre = *ent;
                if (ImGui::SmallButton("o##RPos")) { ent->transform.position = glm::vec3(0.0f); ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Reset Position"); }
                ImGui::SameLine();
                bool ed = ImGui::DragFloat3("Position (m)", &ent->transform.position.x, 0.05f, -5000.0f, 5000.0f, "%.3f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Move Entity", "tr_pos_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                if (ImGui::SmallButton("o##RRot")) { ent->transform.rotation = glm::vec3(0.0f); ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Reset Rotation"); }
                ImGui::SameLine();
                bool ed = ImGui::DragFloat3("Rotation (deg)", &ent->transform.rotation.x, 0.5f, -360.0f, 360.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Rotate Entity", "tr_rot_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                if (ImGui::SmallButton("o##RScl")) { ent->transform.scale = glm::vec3(1.0f); ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Reset Scale"); }
                ImGui::SameLine();
                bool ed = ImGui::DragFloat3("Scale", &ent->transform.scale.x, 0.02f, 0.01f, 500.0f, "%.3f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Scale Entity", "tr_scl_" + std::to_string(ent->id));
            }
        }
    }

    void InspectorPanel::DrawMeshCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Entity preHeader = *ent;
            if (ImGui::Checkbox("Visible##Mesh", &ent->mesh.enabled))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preHeader, *ent, "Toggle Mesh Visibility");
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##Mesh")) { ent->mesh = MeshComponent{}; ent->hasMesh = true; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preHeader, *ent, "Reset Mesh"); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##Mesh")) { ent->hasMesh = false; ent->mesh.enabled = false; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preHeader, *ent, "Remove Mesh"); return; }

            Entity pre = *ent;
            const char* prims[] = { "Cube", "Sphere", "Cylinder", "Plane", "Cone", "Torus", "Custom OBJ" };
            int pIdx = static_cast<int>(ent->mesh.primitive);
            if (ImGui::Combo("Primitive", &pIdx, prims, IM_ARRAYSIZE(prims)))
            {
                ent->mesh.primitive = static_cast<MeshPrimitive>(pIdx);
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Change Mesh Primitive");
            }

            if (ent->mesh.primitive == MeshPrimitive::CustomOBJ)
            {
                Entity prePath = *ent;
                char objBuf[256];
                std::snprintf(objBuf, sizeof(objBuf), "%s", ent->mesh.customObjPath.c_str());
                bool ed = ImGui::InputText("OBJ Path", objBuf, sizeof(objBuf));
                if (ed) ent->mesh.customObjPath = objBuf;
                CommitWidgetEdit(ctx, ent, prePath, ed, "Set Custom OBJ", "mesh_obj_" + std::to_string(ent->id));

                // Drag-drop target for .obj from Asset Browser
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* accept = ImGui::AcceptDragDropPayload("HARGON_ASSET"))
                    {
                        std::string relPath(static_cast<const char*>(accept->Data));
                        if (fs::path(relPath).extension() == ".obj")
                        {
                            Entity beforeDrop = *ent;
                            ent->mesh.primitive = MeshPrimitive::CustomOBJ;
                            ent->mesh.customObjPath = relPath;
                            ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, beforeDrop, *ent, "Assign OBJ Mesh");
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("Browse##Obj") && ctx.projectConfig)
                {
                    std::string picked;
                    if (ProjectManager::BrowseForAssetFile(picked))
                    {
                        std::string importedRel;
                        if (ProjectManager::ImportAssetIntoProject(*ctx.projectConfig, picked, "models", &importedRel))
                        {
                            Entity beforeImp = *ent;
                            ent->mesh.customObjPath = importedRel;
                            ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, beforeImp, *ent, "Import OBJ Mesh");
                        }
                    }
                }
            }

            Entity preSh = *ent;
            if (ImGui::Checkbox("Cast Shadows", &ent->mesh.castShadows))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preSh, *ent, "Toggle Cast Shadows");
        }
    }

    void InspectorPanel::DrawMaterialCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::SmallButton("Copy##Mat")) { ctx.clipboard.type = ComponentClipboard::Type::Material; ctx.clipboard.material = ent->material; ctx.SetToast("Copied Material"); }
            ImGui::SameLine();
            if (ctx.clipboard.type == ComponentClipboard::Type::Material && ImGui::SmallButton("Paste##Mat"))
            {
                Entity pre = *ent;
                ent->material = ctx.clipboard.material;
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Paste Material");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##Mat"))
            {
                Entity pre = *ent;
                ent->material = MaterialComponent{};
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Reset Material");
            }
            if (ent->hasMaterial && !ent->HasMesh())
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##Mat"))
                {
                    Entity pre = *ent;
                    ent->hasMaterial = false;
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Remove Material");
                    return;
                }
            }

            // Quick Presets
            auto presetBtn = [&](const char* lbl, float met, float rough, float emStr) {
                if (ImGui::SmallButton(lbl))
                {
                    Entity pre = *ent;
                    ent->material.metallic = met;
                    ent->material.roughness = rough;
                    ent->material.emissiveStrength = emStr;
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, std::string("Material Preset ") + lbl);
                }
            };
            presetBtn("Matte", 0.0f, 0.85f, 0.0f); ImGui::SameLine();
            presetBtn("Metal", 0.92f, 0.22f, 0.0f); ImGui::SameLine();
            presetBtn("Neon", 0.1f, 0.3f, 3.2f); ImGui::SameLine();
            presetBtn("Stone", 0.05f, 0.90f, 0.0f);

            {
                Entity pre = *ent;
                bool ed = ImGui::ColorEdit3("Albedo", &ent->material.albedoColor.x);
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Albedo", "mat_alb_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::SliderFloat("Metallic", &ent->material.metallic, 0.0f, 1.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Metallic", "mat_met_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::SliderFloat("Roughness", &ent->material.roughness, 0.02f, 1.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Roughness", "mat_rgh_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("UV Scale", &ent->material.uvScale, 0.05f, 0.1f, 32.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit UV Scale", "mat_uv_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragInt("Surface Pattern", &ent->material.patternType, 0.2f, 0, 14);
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Surface Pattern", "mat_pat_" + std::to_string(ent->id));
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                    ImGui::SetTooltip("0=Solid, 1=ArenaGrid, 2=Checker, 3=TechPanels, 4=CrystalFacet, 5..14=Procedural");
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::ColorEdit3("Emissive Color", &ent->material.emissiveColor.x);
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Emissive Color", "mat_emc_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::SliderFloat("Emissive Glow", &ent->material.emissiveStrength, 0.0f, 12.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Emissive Glow", "mat_ems_" + std::to_string(ent->id));
            }
        }
    }

    void InspectorPanel::DrawLightCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Entity preH = *ent;
            if (ImGui::Checkbox("Enabled##Light", &ent->light.enabled))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Toggle Light");
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##Light")) { ent->light = LightComponent{}; ent->light.enabled = true; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Reset Light"); }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##Light")) { ent->hasLight = false; ent->light.enabled = false; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Remove Light"); return; }

            {
                Entity pre = *ent;
                if (ImGui::Checkbox("Directional Sun", &ent->light.isDirectional))
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Toggle Directional Light");
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::ColorEdit3("Color##Light", &ent->light.color.x);
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Light Color", "lgt_col_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Intensity", &ent->light.intensity, 0.1f, 0.0f, 50.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Light Intensity", "lgt_int_" + std::to_string(ent->id));
            }
            if (!ent->light.isDirectional)
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Range (m)", &ent->light.range, 0.2f, 0.5f, 200.0f, "%.1f m");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Light Range", "lgt_rng_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                if (ImGui::Checkbox("Cast Shadows##Light", &ent->light.castShadows))
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Toggle Light Shadows");
            }
        }
    }

    void InspectorPanel::DrawCameraCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("Camera3D", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Entity preH = *ent;
            if (ImGui::Checkbox("Enabled##Cam", &ent->camera.enabled))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Toggle Camera");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##Cam")) { ent->hasCamera = false; ent->camera.enabled = false; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Remove Camera"); return; }

            Entity pre = *ent;
            const char* camModes[] = { "Free Look", "Follow Target", "First Person", "Top Down", "Third Person Orbit" };
            int modeIdx = static_cast<int>(ent->camera.mode);
            if (ImGui::Combo("Mode", &modeIdx, camModes, IM_ARRAYSIZE(camModes)))
            {
                ent->camera.mode = static_cast<CameraMode>(modeIdx);
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Change Camera Mode");
            }
            {
                Entity preF = *ent;
                bool ed = ImGui::SliderFloat("FOV (deg)", &ent->camera.fov, 30.0f, 120.0f, "%.1f deg");
                CommitWidgetEdit(ctx, ent, preF, ed, "Edit Camera FOV", "cam_fov_" + std::to_string(ent->id));
            }
            {
                Entity preO = *ent;
                bool ed = ImGui::DragFloat3("Offset (m)", &ent->camera.followOffset.x, 0.1f, -50.0f, 50.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, preO, ed, "Edit Camera Offset", "cam_off_" + std::to_string(ent->id));
            }
            {
                Entity preA = *ent;
                if (ImGui::Checkbox("Primary Game Camera", &ent->camera.isPrimary))
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preA, *ent, "Toggle Primary Camera");
            }
        }
    }

    void InspectorPanel::DrawRigidBodyCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("RigidBody3D", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Entity preH = *ent;
            if (ImGui::Checkbox("Enabled##RB", &ent->rigidBody.enabled))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Toggle RigidBody");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##RB")) { ent->hasRigidBody = false; ent->rigidBody.enabled = false; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Remove RigidBody"); return; }

            {
                Entity pre = *ent;
                if (ImGui::Checkbox("Kinematic / Static", &ent->rigidBody.isKinematic))
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Toggle Kinematic Body");
            }
            {
                Entity pre = *ent;
                if (ImGui::Checkbox("Use Gravity", &ent->rigidBody.useGravity))
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Toggle Gravity");
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Mass (kg)", &ent->rigidBody.mass, 0.1f, 0.05f, 1000.0f, "%.2f kg");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Mass", "rb_mass_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Linear Drag", &ent->rigidBody.drag, 0.02f, 0.0f, 20.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Drag", "rb_drag_" + std::to_string(ent->id));
            }
        }
    }

    void InspectorPanel::DrawColliderCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("Collider3D", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Entity preH = *ent;
            if (ImGui::Checkbox("Enabled##Col", &ent->collider.enabled))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Toggle Collider");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##Col")) { ent->hasCollider = false; ent->collider.enabled = false; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Remove Collider"); return; }

            Entity pre = *ent;
            const char* shapes[] = { "Box", "Sphere" };
            int sIdx = ent->collider.isSphere ? 1 : 0;
            if (ImGui::Combo("Shape", &sIdx, shapes, IM_ARRAYSIZE(shapes)))
            {
                ent->collider.isSphere = (sIdx == 1);
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Change Collider Shape");
            }

            if (!ent->collider.isSphere)
            {
                Entity preE = *ent;
                bool ed = ImGui::DragFloat3("Half Extents (m)", &ent->collider.boxHalfExtents.x, 0.05f, 0.05f, 200.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, preE, ed, "Edit Collider Extents", "col_ext_" + std::to_string(ent->id));
            }
            else
            {
                Entity preR = *ent;
                bool ed = ImGui::DragFloat("Radius (m)", &ent->collider.sphereRadius, 0.05f, 0.05f, 100.0f, "%.2f m");
                CommitWidgetEdit(ctx, ent, preR, ed, "Edit Collider Radius", "col_rad_" + std::to_string(ent->id));
            }
            {
                Entity preT = *ent;
                if (ImGui::Checkbox("Is Trigger (Area3D)", &ent->collider.isTrigger))
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preT, *ent, "Toggle Trigger");
            }
        }
    }

    void InspectorPanel::DrawRayCastCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("RayCast3D", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Entity preH = *ent;
            if (ImGui::Checkbox("Enabled##Ray", &ent->raycast.enabled))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Toggle RayCast3D");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##Ray")) { ent->hasRayCast = false; ent->raycast.enabled = false; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Remove RayCast3D"); return; }

            Entity pre = *ent;
            bool ed = ImGui::DragFloat3("Target Vector (m)", &ent->raycast.targetPosition.x, 0.1f, -200.0f, 200.0f, "%.2f");
            CommitWidgetEdit(ctx, ent, pre, ed, "Edit RayCast Target", "ray_tgt_" + std::to_string(ent->id));
            ImGui::TextDisabled("Colliding: %s", ent->raycast.isColliding ? "YES" : "No");
        }
    }

    void InspectorPanel::DrawAudioCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("AudioSource3D", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Entity preH = *ent;
            if (ImGui::Checkbox("Enabled##Aud", &ent->audioSource.enabled))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Toggle AudioSource");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##Aud")) { ent->hasAudio = false; ent->audioSource.enabled = false; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Remove AudioSource"); return; }

            {
                Entity pre = *ent;
                char sndBuf[128];
                std::snprintf(sndBuf, sizeof(sndBuf), "%s", ent->audioSource.soundName.c_str());
                bool ed = ImGui::InputText("Sound Name", sndBuf, sizeof(sndBuf));
                if (ed) ent->audioSource.soundName = sndBuf;
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Sound Name", "aud_name_" + std::to_string(ent->id));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* accept = ImGui::AcceptDragDropPayload("HARGON_ASSET"))
                    {
                        std::string relPath(static_cast<const char*>(accept->Data));
                        if (fs::path(relPath).extension() == ".wav")
                        {
                            Entity beforeDrop = *ent;
                            ent->audioSource.soundName = fs::path(relPath).stem().string();
                            ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, beforeDrop, *ent, "Assign Sound");
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::SliderFloat("Volume", &ent->audioSource.volume, 0.0f, 2.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Audio Volume", "aud_vol_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::SliderFloat("Pitch", &ent->audioSource.pitch, 0.25f, 2.5f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Audio Pitch", "aud_ptc_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Max Distance (m)", &ent->audioSource.maxDistance, 1.0f, 2.0f, 250.0f, "%.1f m");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Audio Range", "aud_dst_" + std::to_string(ent->id));
            }
            if (ctx.audio && ImGui::SmallButton("Test Play Sound"))
                ctx.audio->PlaySound2D(ent->audioSource.soundName, ent->audioSource.volume, ent->audioSource.pitch);
        }
    }

    void InspectorPanel::DrawLuaScriptCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("Lua Script", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Entity preH = *ent;
            if (ImGui::Checkbox("Enabled##Lua", &ent->luaScript.enabled))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Toggle Lua Script");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##Lua")) { ent->hasLuaScript = false; ent->luaScript.enabled = false; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Remove Lua Script"); return; }

            Entity pre = *ent;
            char pathBuf[256];
            std::snprintf(pathBuf, sizeof(pathBuf), "%s", ent->luaScript.scriptPath.c_str());
            bool ed = ImGui::InputText("Script Path", pathBuf, sizeof(pathBuf));
            if (ed) ent->luaScript.scriptPath = pathBuf;
            CommitWidgetEdit(ctx, ent, pre, ed, "Edit Lua Script Path", "lua_pth_" + std::to_string(ent->id));

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* accept = ImGui::AcceptDragDropPayload("HARGON_ASSET"))
                {
                    std::string relPath(static_cast<const char*>(accept->Data));
                    if (fs::path(relPath).extension() == ".lua")
                    {
                        Entity beforeDrop = *ent;
                        ent->luaScript.scriptPath = relPath;
                        ent->luaScript.enabled = true;
                        ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, beforeDrop, *ent, "Attach Lua Script");
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (!ent->luaScript.scriptPath.empty())
            {
                if (ImGui::Button("Open in Script Editor"))
                {
                    if (ctx.openScriptTab)
                        ctx.openScriptTab(ent->luaScript.scriptPath, -1);
                }
            }
        }
    }

    void InspectorPanel::DrawParticlesCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("CPUParticles3D", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Entity preH = *ent;
            if (ImGui::Checkbox("Enabled##Part", &ent->particleEmitter.enabled))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Toggle Particles");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##Part")) { ent->hasParticles = false; ent->particleEmitter.enabled = false; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Remove Particles"); return; }

            {
                Entity pre = *ent;
                if (ImGui::Checkbox("Emitting", &ent->particleEmitter.emitting))
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Toggle Emitting");
            }
            {
                Entity pre = *ent;
                const char* shapes[] = { "Point", "Sphere", "Box", "Ring" };
                int sh = ent->particleEmitter.emissionShape;
                if (ImGui::Combo("Emission Shape", &sh, shapes, IM_ARRAYSIZE(shapes)))
                {
                    ent->particleEmitter.emissionShape = sh;
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Change Particle Shape");
                }
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Rate (/s)", &ent->particleEmitter.emissionRate, 0.5f, 1.0f, 300.0f, "%.1f /s");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Emission Rate", "prt_rte_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Speed (m/s)", &ent->particleEmitter.initialSpeed, 0.2f, 0.1f, 50.0f, "%.1f m/s");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Particle Speed", "prt_spd_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::ColorEdit3("Start Color", &ent->particleEmitter.startColor.x);
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Particle Start Color", "prt_col_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::ColorEdit3("End Color", &ent->particleEmitter.endColor.x);
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Particle End Color", "prt_ecol_" + std::to_string(ent->id));
            }
        }
    }

    void InspectorPanel::DrawAnimationCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("AnimationPlayer", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Entity preH = *ent;
            if (ImGui::Checkbox("Enabled##Anim", &ent->animation.enabled))
                ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Toggle AnimationPlayer");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##Anim")) { ent->hasAnimation = false; ent->animation.enabled = false; ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, preH, *ent, "Remove AnimationPlayer"); return; }

            {
                Entity pre = *ent;
                if (ImGui::Checkbox("Playing", &ent->animation.playing))
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Toggle Animation Playing");
            }
            {
                Entity pre = *ent;
                const char* tracks[] = { "IdleBob", "SpinContinuous", "PatrolPingPong", "PulseScale", "OrbitCircle", "PendulumSwing" };
                int tIdx = std::clamp(ent->animation.trackType, 0, 5);
                if (ImGui::Combo("Animation Track", &tIdx, tracks, IM_ARRAYSIZE(tracks)))
                {
                    ent->animation.trackType = tIdx;
                    ent->animation.animName = tracks[tIdx];
                    ctx.commands->RecordEntityEdit(*ctx.scene, ent->id, pre, *ent, "Change Animation Track");
                }
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Playback Speed", &ent->animation.speed, 0.05f, -5.0f, 5.0f, "%.2fx");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Animation Speed", "anm_spd_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Amplitude", &ent->animation.amplitude, 0.05f, 0.0f, 20.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Animation Amplitude", "anm_amp_" + std::to_string(ent->id));
            }
        }
    }

    void InspectorPanel::DrawGameplayCard(EditorContext& ctx, Entity* ent)
    {
        if (ImGui::CollapsingHeader("Gameplay Attributes"))
        {
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Health", &ent->gameplay.health, 1.0f, 0.0f, 10000.0f, "%.0f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Health", "gp_hp_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Max Health", &ent->gameplay.maxHealth, 1.0f, 1.0f, 10000.0f, "%.0f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Max Health", "gp_mhp_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragFloat("Move Speed (m/s)", &ent->gameplay.moveSpeed, 0.25f, 0.0f, 100.0f, "%.2f");
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Move Speed", "gp_spd_" + std::to_string(ent->id));
            }
            {
                Entity pre = *ent;
                bool ed = ImGui::DragInt("Score Value", &ent->gameplay.scoreValue, 5.0f, -10000, 100000);
                CommitWidgetEdit(ctx, ent, pre, ed, "Edit Score Value", "gp_scr_" + std::to_string(ent->id));
            }
        }
    }

    void InspectorPanel::DrawWorldEnvironment(EditorContext& ctx)
    {
        if (!ctx.showWorldEnv)
            return;

        if (!ImGui::Begin(kWinWorldEnv, &ctx.showWorldEnv))
        {
            ImGui::End();
            return;
        }

        if (!ctx.scene)
        {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        Scene* sc = ctx.scene;
        ImGui::PushItemWidth(std::max(100.0f, ImGui::GetContentRegionAvail().x * 0.54f));

        if (ImGui::CollapsingHeader("Scene Sky & Fog", ImGuiTreeNodeFlags_DefaultOpen))
        {
            {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                bool ed = ImGui::ColorEdit3("Sky Zenith", &sc->skyZenithColor.x);
                CommitEnvWidgetEdit(ctx, pre, ed, "Edit Sky Zenith", "env_zenith");
            }
            {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                bool ed = ImGui::ColorEdit3("Sky Horizon", &sc->skyHorizonColor.x);
                CommitEnvWidgetEdit(ctx, pre, ed, "Edit Sky Horizon", "env_horizon");
            }
            {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                bool ed = ImGui::ColorEdit3("Ground Ambient", &sc->groundAmbientColor.x);
                CommitEnvWidgetEdit(ctx, pre, ed, "Edit Ground Ambient", "env_ground");
            }
            {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                bool ed = ImGui::SliderFloat("Ambient Intensity", &sc->ambientIntensity, 0.0f, 3.0f, "%.2f");
                CommitEnvWidgetEdit(ctx, pre, ed, "Edit Ambient Intensity", "env_amb");
            }
            {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                bool ed = ImGui::SliderFloat("Fog Density", &sc->fogDensity, 0.0f, 0.05f, "%.4f");
                CommitEnvWidgetEdit(ctx, pre, ed, "Edit Fog Density", "env_fogd");
            }
        }

        if (ImGui::CollapsingHeader("Post-Processing & Tonemapping", ImGuiTreeNodeFlags_DefaultOpen))
        {
            {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                if (ImGui::Checkbox("Enable SSAO", &sc->worldEnv.ssaoEnabled))
                    ctx.commands->RecordEnvironmentEdit(*sc, pre, SceneEnvSnapshot::Capture(*sc), "Toggle SSAO");
            }
            {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                bool ed = ImGui::SliderFloat("SSAO Radius (m)", &sc->worldEnv.ssaoRadius, 0.1f, 3.0f, "%.2f m");
                CommitEnvWidgetEdit(ctx, pre, ed, "Edit SSAO Radius", "env_ssaor");
            }
            {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                bool ed = ImGui::SliderFloat("SSAO Intensity", &sc->worldEnv.ssaoIntensity, 0.0f, 3.0f, "%.2f");
                CommitEnvWidgetEdit(ctx, pre, ed, "Edit SSAO Intensity", "env_ssaoi");
            }
            {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                const char* tms[] = { "Linear", "Reinhard", "Filmic", "ACES", "AgX" };
                int tm = static_cast<int>(sc->worldEnv.tonemapMode);
                if (ImGui::Combo("Tonemapper", &tm, tms, IM_ARRAYSIZE(tms)))
                {
                    sc->worldEnv.tonemapMode = static_cast<ToneMapperMode>(tm);
                    ctx.commands->RecordEnvironmentEdit(*sc, pre, SceneEnvSnapshot::Capture(*sc), "Change Tonemapper");
                }
            }
            {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                bool ed = ImGui::SliderFloat("Exposure", &sc->worldEnv.tonemapExposure, 0.2f, 4.0f, "%.2f");
                CommitEnvWidgetEdit(ctx, pre, ed, "Edit Exposure", "env_exp");
            }
        }

        if (ImGui::CollapsingHeader("Scene Title & HUD Labels"))
        {
            auto textEdit = [&](const char* label, std::string& field, const std::string& key) {
                SceneEnvSnapshot pre = SceneEnvSnapshot::Capture(*sc);
                char buf[256];
                std::snprintf(buf, sizeof(buf), "%s", field.c_str());
                bool ed = ImGui::InputText(label, buf, sizeof(buf));
                if (ed) field = buf;
                CommitEnvWidgetEdit(ctx, pre, ed, std::string("Edit ") + label, key);
            };
            textEdit("Scene Name", sc->sceneName, "env_scname");
            textEdit("Intro Banner", sc->introBannerText, "env_intro");
            textEdit("Controls Hint", sc->hudControlsText, "env_ctrls");
        }

        ImGui::PopItemWidth();
        ImGui::End();
    }
}
