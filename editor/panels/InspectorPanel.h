#pragma once
#include "../EditorContext.h"
#include <string>

namespace Hargon
{
    class InspectorPanel
    {
    public:
        void Draw(EditorContext& ctx);
        void DrawWorldEnvironment(EditorContext& ctx);

    private:
        bool MatchesFilter(const char* sectionName, const char* keywords) const;
        void CommitWidgetEdit(
            EditorContext& ctx,
            Entity* ent,
            const Entity& preWidgetState,
            bool edited,
            const std::string& label,
            const std::string& mergeKey);

        void CommitEnvWidgetEdit(
            EditorContext& ctx,
            const SceneEnvSnapshot& preState,
            bool edited,
            const std::string& label,
            const std::string& mergeKey);

        void DrawTransformCard(EditorContext& ctx, Entity* ent);
        void DrawMeshCard(EditorContext& ctx, Entity* ent);
        void DrawMaterialCard(EditorContext& ctx, Entity* ent);
        void DrawLightCard(EditorContext& ctx, Entity* ent);
        void DrawCameraCard(EditorContext& ctx, Entity* ent);
        void DrawRigidBodyCard(EditorContext& ctx, Entity* ent);
        void DrawColliderCard(EditorContext& ctx, Entity* ent);
        void DrawRayCastCard(EditorContext& ctx, Entity* ent);
        void DrawAudioCard(EditorContext& ctx, Entity* ent);
        void DrawLuaScriptCard(EditorContext& ctx, Entity* ent);
        void DrawParticlesCard(EditorContext& ctx, Entity* ent);
        void DrawAnimationCard(EditorContext& ctx, Entity* ent);
        void DrawGameplayCard(EditorContext& ctx, Entity* ent);

    private:
        char m_PropSearchBuf[128] = "";
        char m_NewGroupBuf[64] = "";

        std::string m_ActiveMergeKey;
        Entity m_DragStartEntity{};
        bool m_HasDragStart = false;

        std::string m_EnvMergeKey;
        SceneEnvSnapshot m_EnvDragStart{};
        bool m_HasEnvDragStart = false;
    };
}
