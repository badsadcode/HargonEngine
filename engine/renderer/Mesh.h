#pragma once
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstdint>

namespace Hargon
{
    struct Vertex3D
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    enum class MeshPrimitive
    {
        Cube = 0,
        Sphere,
        Cylinder,
        Cone,
        Torus,
        Plane,
        Rover,
        Drone,
        Crystal,
        Turret,
        Core,
        CustomOBJ,
        // 10 FPS Enemy Types
        EnemyGrunt,
        EnemyEnforcer,
        EnemyRottweiler,
        EnemyKnight,
        EnemyHellKnight,
        EnemyOgre,
        EnemyFiend,
        EnemyScrag,
        EnemyVore,
        EnemyShambler,
        // 5 FPS Weapons
        WeaponAxe,
        WeaponShotgun,
        WeaponSuperNailgun,
        WeaponRocketLauncher,
        WeaponLightningGun,
        // Atmospheric Decorations & Pickups
        PropTorch,
        PropBrazier,
        PropBarrel,
        PropPillar,
        PropObelisk,
        PropCage,
        PropCrate,
        PropAltar,
        PropSlipgate,
        PropGib,
        Count
    };

    struct GPUMesh
    {
        std::string name;
        std::vector<Vertex3D> vertices;
        std::vector<uint32_t> indices;
        nvrhi::BufferHandle vertexBuffer;
        nvrhi::BufferHandle indexBuffer;
        uint32_t indexCount = 0;
        glm::vec3 boundsMin{ -0.5f };
        glm::vec3 boundsMax{ 0.5f };
    };

    class MeshFactory
    {
    public:
        static GPUMesh CreatePrimitive(nvrhi::IDevice* device, nvrhi::ICommandList* cmd, MeshPrimitive type);
        static GPUMesh LoadOBJ(nvrhi::IDevice* device, nvrhi::ICommandList* cmd, const std::string& objPath);
        static void UploadMeshBuffers(nvrhi::IDevice* device, nvrhi::ICommandList* cmd, GPUMesh& mesh);

        static const char* PrimitiveToString(MeshPrimitive type);
        static MeshPrimitive StringToPrimitive(const std::string& str);

    private:
        static void AppendBox(std::vector<Vertex3D>& verts, std::vector<uint32_t>& idx, const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color);
        static void AppendCylinder(std::vector<Vertex3D>& verts, std::vector<uint32_t>& idx, const glm::vec3& center, float radiusTop, float radiusBottom, float height, int slices, const glm::vec4& color, bool alignZ = false);
        static void AppendSphere(std::vector<Vertex3D>& verts, std::vector<uint32_t>& idx, const glm::vec3& center, float radius, int stacks, int slices, const glm::vec4& color);
    };
}
