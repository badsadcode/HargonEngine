#pragma once
#include "../scene/Scene.h"
#include <string>

namespace Hargon
{
    class SceneTemplates
    {
    public:
        static void BuildEmptyTemplateScene(Scene& scene, const std::string& title = "New 3D Scene");
        static void BuildStarforgeOutpostScene(Scene& scene);
        static void BuildAetherRealmsScene(Scene& scene);
        static void BuildSkywayCourseScene(Scene& scene, const std::string& title = "Skyway 3D Time-Trial");

        static Entity& SpawnEnemyDrone(Scene& scene, const glm::vec3& pos, int waveLevel = 1);
        static Entity& SpawnEnergyCrystal(Scene& scene, const glm::vec3& pos, bool golden = false);
        static Entity& SpawnProjectile(
            Scene& scene,
            const glm::vec3& pos,
            const glm::vec3& dir,
            EntityID ownerId,
            bool isEnemyProjectile = false,
            float speed = 38.0f,
            float damage = 25.0f);
    };
}
