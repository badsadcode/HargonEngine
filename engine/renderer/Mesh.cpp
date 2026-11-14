#include "Mesh.h"
#include "../core/Log.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Hargon
{
    static constexpr float kPI = 3.14159265358979323846f;

    const char* MeshFactory::PrimitiveToString(MeshPrimitive type)
    {
        switch (type)
        {
        case MeshPrimitive::Cube:      return "Cube";
        case MeshPrimitive::Sphere:    return "Sphere";
        case MeshPrimitive::Cylinder:  return "Cylinder";
        case MeshPrimitive::Cone:      return "Cone";
        case MeshPrimitive::Torus:     return "Torus";
        case MeshPrimitive::Plane:     return "Plane";
        case MeshPrimitive::Rover:     return "Rover";
        case MeshPrimitive::Drone:     return "Drone";
        case MeshPrimitive::Crystal:   return "Crystal";
        case MeshPrimitive::Turret:    return "Turret";
        case MeshPrimitive::Core:      return "Core";
        case MeshPrimitive::CustomOBJ: return "CustomOBJ";
        case MeshPrimitive::EnemyGrunt:          return "EnemyGrunt";
        case MeshPrimitive::EnemyEnforcer:       return "EnemyEnforcer";
        case MeshPrimitive::EnemyRottweiler:     return "EnemyRottweiler";
        case MeshPrimitive::EnemyKnight:         return "EnemyKnight";
        case MeshPrimitive::EnemyHellKnight:     return "EnemyHellKnight";
        case MeshPrimitive::EnemyOgre:           return "EnemyOgre";
        case MeshPrimitive::EnemyFiend:          return "EnemyFiend";
        case MeshPrimitive::EnemyScrag:          return "EnemyScrag";
        case MeshPrimitive::EnemyVore:           return "EnemyVore";
        case MeshPrimitive::EnemyShambler:       return "EnemyShambler";
        case MeshPrimitive::WeaponAxe:           return "WeaponAxe";
        case MeshPrimitive::WeaponShotgun:       return "WeaponShotgun";
        case MeshPrimitive::WeaponSuperNailgun:  return "WeaponSuperNailgun";
        case MeshPrimitive::WeaponRocketLauncher:return "WeaponRocketLauncher";
        case MeshPrimitive::WeaponLightningGun:  return "WeaponLightningGun";
        case MeshPrimitive::PropTorch:           return "PropTorch";
        case MeshPrimitive::PropBrazier:         return "PropBrazier";
        case MeshPrimitive::PropBarrel:          return "PropBarrel";
        case MeshPrimitive::PropPillar:          return "PropPillar";
        case MeshPrimitive::PropObelisk:         return "PropObelisk";
        case MeshPrimitive::PropCage:            return "PropCage";
        case MeshPrimitive::PropCrate:           return "PropCrate";
        case MeshPrimitive::PropAltar:           return "PropAltar";
        case MeshPrimitive::PropSlipgate:        return "PropSlipgate";
        case MeshPrimitive::PropGib:             return "PropGib";
        default:                       return "Cube";
        }
    }

    MeshPrimitive MeshFactory::StringToPrimitive(const std::string& str)
    {
        if (str == "Sphere")    return MeshPrimitive::Sphere;
        if (str == "Cylinder")  return MeshPrimitive::Cylinder;
        if (str == "Cone")      return MeshPrimitive::Cone;
        if (str == "Torus")     return MeshPrimitive::Torus;
        if (str == "Plane")     return MeshPrimitive::Plane;
        if (str == "Rover")     return MeshPrimitive::Rover;
        if (str == "Drone")     return MeshPrimitive::Drone;
        if (str == "Crystal")   return MeshPrimitive::Crystal;
        if (str == "Turret")    return MeshPrimitive::Turret;
        if (str == "Core")      return MeshPrimitive::Core;
        if (str == "CustomOBJ") return MeshPrimitive::CustomOBJ;
        if (str == "EnemyGrunt")          return MeshPrimitive::EnemyGrunt;
        if (str == "EnemyEnforcer")       return MeshPrimitive::EnemyEnforcer;
        if (str == "EnemyRottweiler")     return MeshPrimitive::EnemyRottweiler;
        if (str == "EnemyKnight")         return MeshPrimitive::EnemyKnight;
        if (str == "EnemyHellKnight")     return MeshPrimitive::EnemyHellKnight;
        if (str == "EnemyOgre")           return MeshPrimitive::EnemyOgre;
        if (str == "EnemyFiend")          return MeshPrimitive::EnemyFiend;
        if (str == "EnemyScrag")          return MeshPrimitive::EnemyScrag;
        if (str == "EnemyVore")           return MeshPrimitive::EnemyVore;
        if (str == "EnemyShambler")       return MeshPrimitive::EnemyShambler;
        if (str == "WeaponAxe")           return MeshPrimitive::WeaponAxe;
        if (str == "WeaponShotgun")       return MeshPrimitive::WeaponShotgun;
        if (str == "WeaponSuperNailgun")  return MeshPrimitive::WeaponSuperNailgun;
        if (str == "WeaponRocketLauncher")return MeshPrimitive::WeaponRocketLauncher;
        if (str == "WeaponLightningGun")  return MeshPrimitive::WeaponLightningGun;
        if (str == "PropTorch")           return MeshPrimitive::PropTorch;
        if (str == "PropBrazier")         return MeshPrimitive::PropBrazier;
        if (str == "PropBarrel")          return MeshPrimitive::PropBarrel;
        if (str == "PropPillar")          return MeshPrimitive::PropPillar;
        if (str == "PropObelisk")         return MeshPrimitive::PropObelisk;
        if (str == "PropCage")            return MeshPrimitive::PropCage;
        if (str == "PropCrate")           return MeshPrimitive::PropCrate;
        if (str == "PropAltar")           return MeshPrimitive::PropAltar;
        if (str == "PropSlipgate")        return MeshPrimitive::PropSlipgate;
        if (str == "PropGib")             return MeshPrimitive::PropGib;
        return MeshPrimitive::Cube;
    }

    void MeshFactory::AppendBox(
        std::vector<Vertex3D>& verts,
        std::vector<uint32_t>& idx,
        const glm::vec3& c,
        const glm::vec3& h,
        const glm::vec4& col)
    {
        uint32_t base = static_cast<uint32_t>(verts.size());

        struct Face
        {
            glm::vec3 normal;
            glm::vec3 v0, v1, v2, v3;
        };

        Face faces[6] = {
            // +Z (Front)
            { {0, 0, 1},  {-h.x, -h.y,  h.z}, { h.x, -h.y,  h.z}, { h.x,  h.y,  h.z}, {-h.x,  h.y,  h.z} },
            // -Z (Back)
            { {0, 0,-1},  { h.x, -h.y, -h.z}, {-h.x, -h.y, -h.z}, {-h.x,  h.y, -h.z}, { h.x,  h.y, -h.z} },
            // +Y (Top)
            { {0, 1, 0},  {-h.x,  h.y,  h.z}, { h.x,  h.y,  h.z}, { h.x,  h.y, -h.z}, {-h.x,  h.y, -h.z} },
            // -Y (Bottom)
            { {0,-1, 0},  {-h.x, -h.y, -h.z}, { h.x, -h.y, -h.z}, { h.x, -h.y,  h.z}, {-h.x, -h.y,  h.z} },
            // +X (Right)
            { {1, 0, 0},  { h.x, -h.y,  h.z}, { h.x, -h.y, -h.z}, { h.x,  h.y, -h.z}, { h.x,  h.y,  h.z} },
            // -X (Left)
            { {-1,0, 0},  {-h.x, -h.y, -h.z}, {-h.x, -h.y,  h.z}, {-h.x,  h.y,  h.z}, {-h.x,  h.y, -h.z} }
        };

        for (int f = 0; f < 6; ++f)
        {
            uint32_t fBase = base + f * 4;
            verts.push_back({ c + faces[f].v0, faces[f].normal, {0.0f, 1.0f}, col });
            verts.push_back({ c + faces[f].v1, faces[f].normal, {1.0f, 1.0f}, col });
            verts.push_back({ c + faces[f].v2, faces[f].normal, {1.0f, 0.0f}, col });
            verts.push_back({ c + faces[f].v3, faces[f].normal, {0.0f, 0.0f}, col });

            idx.push_back(fBase + 0);
            idx.push_back(fBase + 1);
            idx.push_back(fBase + 2);
            idx.push_back(fBase + 0);
            idx.push_back(fBase + 2);
            idx.push_back(fBase + 3);
        }
    }

    void MeshFactory::AppendSphere(
        std::vector<Vertex3D>& verts,
        std::vector<uint32_t>& idx,
        const glm::vec3& center,
        float radius,
        int stacks,
        int slices,
        const glm::vec4& color)
    {
        uint32_t base = static_cast<uint32_t>(verts.size());

        for (int i = 0; i <= stacks; ++i)
        {
            float v = static_cast<float>(i) / static_cast<float>(stacks);
            float phi = v * kPI;

            for (int j = 0; j <= slices; ++j)
            {
                float u = static_cast<float>(j) / static_cast<float>(slices);
                float theta = u * 2.0f * kPI;

                glm::vec3 n(
                    std::sin(phi) * std::cos(theta),
                    std::cos(phi),
                    std::sin(phi) * std::sin(theta));

                verts.push_back({ center + n * radius, n, {u, v}, color });
            }
        }

        for (int i = 0; i < stacks; ++i)
        {
            for (int j = 0; j < slices; ++j)
            {
                uint32_t first = base + i * (slices + 1) + j;
                uint32_t second = first + slices + 1;

                idx.push_back(first);
                idx.push_back(second);
                idx.push_back(first + 1);

                idx.push_back(second);
                idx.push_back(second + 1);
                idx.push_back(first + 1);
            }
        }
    }

    void MeshFactory::AppendCylinder(
        std::vector<Vertex3D>& verts,
        std::vector<uint32_t>& idx,
        const glm::vec3& center,
        float radiusTop,
        float radiusBottom,
        float height,
        int slices,
        const glm::vec4& color,
        bool alignZ)
    {
        uint32_t base = static_cast<uint32_t>(verts.size());
        float halfH = height * 0.5f;

        auto orient = [alignZ](const glm::vec3& p) -> glm::vec3 {
            if (!alignZ) return p;
            return glm::vec3(p.x, -p.z, p.y);
        };

        for (int i = 0; i <= slices; ++i)
        {
            float u = static_cast<float>(i) / static_cast<float>(slices);
            float theta = u * 2.0f * kPI;
            float c = std::cos(theta);
            float s = std::sin(theta);

            glm::vec3 n = glm::normalize(glm::vec3(c, (radiusBottom - radiusTop) / std::max(height, 0.001f), s));
            glm::vec3 pTop(c * radiusTop, halfH, s * radiusTop);
            glm::vec3 pBot(c * radiusBottom, -halfH, s * radiusBottom);

            verts.push_back({ center + orient(pTop), orient(n), {u, 0.0f}, color });
            verts.push_back({ center + orient(pBot), orient(n), {u, 1.0f}, color });
        }

        for (int i = 0; i < slices; ++i)
        {
            uint32_t i0 = base + i * 2;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + 2;
            uint32_t i3 = i0 + 3;

            idx.push_back(i0);
            idx.push_back(i1);
            idx.push_back(i2);

            idx.push_back(i2);
            idx.push_back(i1);
            idx.push_back(i3);
        }

        // Top cap
        if (radiusTop > 0.0001f)
        {
            uint32_t capCenter = static_cast<uint32_t>(verts.size());
            verts.push_back({ center + orient(glm::vec3(0.0f, halfH, 0.0f)), orient(glm::vec3(0.0f, 1.0f, 0.0f)), {0.5f, 0.5f}, color });
            for (int i = 0; i <= slices; ++i)
            {
                float theta = (static_cast<float>(i) / slices) * 2.0f * kPI;
                glm::vec3 p(std::cos(theta) * radiusTop, halfH, std::sin(theta) * radiusTop);
                verts.push_back({ center + orient(p), orient(glm::vec3(0.0f, 1.0f, 0.0f)), {0.5f, 0.5f}, color });
            }
            for (int i = 0; i < slices; ++i)
            {
                idx.push_back(capCenter);
                idx.push_back(capCenter + 1 + i + 1);
                idx.push_back(capCenter + 1 + i);
            }
        }

        // Bottom cap
        if (radiusBottom > 0.0001f)
        {
            uint32_t capCenter = static_cast<uint32_t>(verts.size());
            verts.push_back({ center + orient(glm::vec3(0.0f, -halfH, 0.0f)), orient(glm::vec3(0.0f, -1.0f, 0.0f)), {0.5f, 0.5f}, color });
            for (int i = 0; i <= slices; ++i)
            {
                float theta = (static_cast<float>(i) / slices) * 2.0f * kPI;
                glm::vec3 p(std::cos(theta) * radiusBottom, -halfH, std::sin(theta) * radiusBottom);
                verts.push_back({ center + orient(p), orient(glm::vec3(0.0f, -1.0f, 0.0f)), {0.5f, 0.5f}, color });
            }
            for (int i = 0; i < slices; ++i)
            {
                idx.push_back(capCenter);
                idx.push_back(capCenter + 1 + i);
                idx.push_back(capCenter + 1 + i + 1);
            }
        }
    }

    GPUMesh MeshFactory::CreatePrimitive(nvrhi::IDevice* device, nvrhi::ICommandList* cmd, MeshPrimitive type)
    {
        GPUMesh mesh;
        mesh.name = PrimitiveToString(type);
        glm::vec4 white(1.0f);

        switch (type)
        {
        case MeshPrimitive::Cube:
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f), glm::vec3(0.5f), white);
            break;

        case MeshPrimitive::Sphere:
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f), 0.5f, 20, 28, white);
            break;

        case MeshPrimitive::Cylinder:
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f), 0.5f, 0.5f, 1.0f, 24, white);
            break;

        case MeshPrimitive::Cone:
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f), 0.0f, 0.5f, 1.0f, 24, white);
            break;

        case MeshPrimitive::Torus:
        {
            int mainSegs = 28;
            int tubeSegs = 16;
            float mainR = 0.36f;
            float tubeR = 0.14f;
            for (int i = 0; i <= mainSegs; ++i)
            {
                float u = static_cast<float>(i) / mainSegs;
                float theta = u * 2.0f * kPI;
                glm::vec3 ringCenter(std::cos(theta) * mainR, 0.0f, std::sin(theta) * mainR);

                for (int j = 0; j <= tubeSegs; ++j)
                {
                    float v = static_cast<float>(j) / tubeSegs;
                    float phi = v * 2.0f * kPI;
                    glm::vec3 n(
                        std::cos(theta) * std::cos(phi),
                        std::sin(phi),
                        std::sin(theta) * std::cos(phi));
                    mesh.vertices.push_back({ ringCenter + n * tubeR, n, {u, v}, white });
                }
            }
            for (int i = 0; i < mainSegs; ++i)
            {
                for (int j = 0; j < tubeSegs; ++j)
                {
                    uint32_t i0 = i * (tubeSegs + 1) + j;
                    uint32_t i1 = i0 + tubeSegs + 1;
                    mesh.indices.push_back(i0);
                    mesh.indices.push_back(i1);
                    mesh.indices.push_back(i0 + 1);
                    mesh.indices.push_back(i1);
                    mesh.indices.push_back(i1 + 1);
                    mesh.indices.push_back(i0 + 1);
                }
            }
            break;
        }

        case MeshPrimitive::Plane:
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.02f, 0.0f), glm::vec3(0.5f, 0.02f, 0.5f), white);
            break;

        case MeshPrimitive::Rover:
        {
            // Cyber Hover-Rover: Hull + Cockpit + 4 Thruster Pods + Dual Plasma Cannons
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.55f, 0.18f, 0.78f), glm::vec4(0.85f, 0.92f, 1.0f, 1.0f));
            // Sloped nose bumper
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.04f, 0.82f), glm::vec3(0.42f, 0.10f, 0.18f), glm::vec4(0.6f, 0.75f, 0.9f, 1.0f));
            // Glowing canopy
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.24f, -0.05f), glm::vec3(0.34f, 0.14f, 0.38f), glm::vec4(0.25f, 0.95f, 1.0f, 1.0f));
            // 4 Hover Thruster Pods
            glm::vec4 podCol(0.30f, 0.35f, 0.42f, 1.0f);
            glm::vec4 glowCol(0.15f, 0.95f, 1.0f, 1.0f);
            float xs[2] = { -0.68f, 0.68f };
            float zs[2] = { -0.52f, 0.52f };
            for (float x : xs)
            {
                for (float z : zs)
                {
                    AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(x, -0.05f, z), 0.22f, 0.18f, 0.24f, 16, podCol);
                    AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(x, -0.18f, z), 0.15f, 0.12f, 0.06f, 16, glowCol);
                }
            }
            // Twin Plasma Barrels facing +Z
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(-0.22f, 0.26f, 0.55f), 0.06f, 0.06f, 0.75f, 12, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f), true);
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3( 0.22f, 0.26f, 0.55f), 0.06f, 0.06f, 0.75f, 12, glm::vec4(0.9f, 0.95f, 1.0f, 1.0f), true);
            break;
        }

        case MeshPrimitive::Drone:
        {
            // Enemy Seeker Drone: Core sphere + 4 aggressive swept wing blades + glowing eye
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.0f, 0.0f), 0.42f, 14, 18, glm::vec4(0.85f, 0.22f, 0.25f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.05f, 0.36f), 0.18f, 10, 12, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.55f, 0.0f, -0.05f), glm::vec3(0.32f, 0.06f, 0.42f), glm::vec4(0.45f, 0.12f, 0.15f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.55f, 0.0f, -0.05f), glm::vec3(0.32f, 0.06f, 0.42f), glm::vec4(0.45f, 0.12f, 0.15f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.32f, 0.0f), 0.03f, 0.05f, 0.35f, 8, glm::vec4(1.0f, 0.3f, 0.3f, 1.0f));
            break;
        }

        case MeshPrimitive::Crystal:
        {
            // Octahedral Energy Crystal cluster
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.30f, 0.0f), 0.0f, 0.38f, 0.60f, 6, glm::vec4(0.35f, 0.95f, 1.0f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.30f, 0.0f), 0.38f, 0.0f, 0.60f, 6, glm::vec4(0.20f, 0.80f, 0.95f, 1.0f));
            // Side crystal shards
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.26f, 0.05f, 0.12f), 0.0f, 0.16f, 0.45f, 5, glm::vec4(0.55f, 1.0f, 1.0f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(-0.24f, -0.02f, -0.15f), 0.0f, 0.15f, 0.42f, 5, glm::vec4(0.45f, 0.9f, 1.0f, 1.0f));
            break;
        }

        case MeshPrimitive::Turret:
        {
            // Defense Turret
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.35f, 0.0f), 0.42f, 0.52f, 0.30f, 16, glm::vec4(0.45f, 0.50f, 0.58f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.05f, 0.0f), 0.25f, 0.28f, 0.35f, 14, glm::vec4(0.60f, 0.68f, 0.75f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.25f, 0.0f), glm::vec3(0.34f, 0.20f, 0.38f), glm::vec4(0.80f, 0.88f, 0.95f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(-0.15f, 0.26f, 0.48f), 0.055f, 0.055f, 0.65f, 10, glm::vec4(0.3f, 0.95f, 0.7f, 1.0f), true);
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3( 0.15f, 0.26f, 0.48f), 0.055f, 0.055f, 0.65f, 10, glm::vec4(0.3f, 0.95f, 0.7f, 1.0f), true);
            break;
        }

        case MeshPrimitive::Core:
        {
            // Central Starforge Reactor Core
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.42f, 0.0f), 0.75f, 0.88f, 0.18f, 20, glm::vec4(0.35f, 0.42f, 0.52f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.12f, 0.0f), 0.42f, 18, 24, glm::vec4(0.25f, 0.85f, 1.0f, 1.0f));
            for (int k = 0; k < 4; ++k)
            {
                float a = k * (kPI * 0.5f);
                glm::vec3 p(std::cos(a) * 0.62f, 0.05f, std::sin(a) * 0.62f);
                AppendBox(mesh.vertices, mesh.indices, p, glm::vec3(0.10f, 0.52f, 0.10f), glm::vec4(0.65f, 0.75f, 0.88f, 1.0f));
            }
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.62f, 0.0f), 0.52f, 0.68f, 0.14f, 20, glm::vec4(0.35f, 0.42f, 0.52f, 1.0f));
            break;
        }

        // ==================== 10 QUAKE-INSPIRED ENEMY TYPES ====================
        case MeshPrimitive::EnemyGrunt:
        {
            // 1. Rotted Grunt (Armored Zombie Shotgunner)
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.14f, -0.45f, 0.0f), glm::vec3(0.09f, 0.30f, 0.10f), glm::vec4(0.28f, 0.24f, 0.18f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.14f, -0.45f, 0.0f), glm::vec3(0.09f, 0.30f, 0.10f), glm::vec4(0.28f, 0.24f, 0.18f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.05f, 0.0f), glm::vec3(0.26f, 0.28f, 0.16f), glm::vec4(0.45f, 0.38f, 0.26f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.10f, 0.06f), glm::vec3(0.22f, 0.18f, 0.14f), glm::vec4(0.55f, 0.22f, 0.18f, 1.0f)); // bloodied chestplate
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.48f, 0.02f), 0.16f, 10, 12, glm::vec4(0.62f, 0.52f, 0.42f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.54f, 0.0f), 0.18f, 0.19f, 0.12f, 12, glm::vec4(0.32f, 0.35f, 0.30f, 1.0f)); // helmet
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.28f, 0.08f, 0.15f), glm::vec3(0.08f, 0.10f, 0.24f), glm::vec4(0.52f, 0.42f, 0.35f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.26f, 0.06f, 0.42f), 0.045f, 0.045f, 0.55f, 8, glm::vec4(0.22f, 0.22f, 0.24f, 1.0f), true); // shotgun
            break;
        }
        case MeshPrimitive::EnemyEnforcer:
        {
            // 2. Enforcer (Heavy Cyber-Armor Laser Trooper)
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.16f, -0.45f, 0.0f), glm::vec3(0.11f, 0.30f, 0.12f), glm::vec4(0.30f, 0.35f, 0.32f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.16f, -0.45f, 0.0f), glm::vec3(0.11f, 0.30f, 0.12f), glm::vec4(0.30f, 0.35f, 0.32f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.08f, 0.0f), glm::vec3(0.32f, 0.30f, 0.20f), glm::vec4(0.38f, 0.46f, 0.36f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(-0.34f, 0.28f, 0.0f), 0.15f, 8, 10, glm::vec4(0.48f, 0.54f, 0.42f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3( 0.34f, 0.28f, 0.0f), 0.15f, 8, 10, glm::vec4(0.48f, 0.54f, 0.42f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.52f, 0.03f), 0.17f, 10, 12, glm::vec4(0.25f, 0.30f, 0.28f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.53f, 0.15f), glm::vec3(0.13f, 0.05f, 0.06f), glm::vec4(1.0f, 0.45f, 0.10f, 1.0f)); // glowing visor
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.32f, 0.08f, 0.42f), 0.06f, 0.07f, 0.62f, 10, glm::vec4(0.85f, 0.55f, 0.18f, 1.0f), true); // blaster
            break;
        }
        case MeshPrimitive::EnemyRottweiler:
        {
            // 3. Rottweiler / War Hound (Quadruped Leaper Beast)
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.18f, 0.0f), glm::vec3(0.22f, 0.18f, 0.42f), glm::vec4(0.48f, 0.28f, 0.18f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.04f, 0.46f), glm::vec3(0.17f, 0.15f, 0.20f), glm::vec4(0.55f, 0.32f, 0.20f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.12f, 0.64f), glm::vec3(0.12f, 0.07f, 0.12f), glm::vec4(0.75f, 0.18f, 0.15f, 1.0f)); // bloodied jaws
            float lx[2] = { -0.16f, 0.16f };
            float lz[2] = { -0.30f, 0.30f };
            for (float x : lx)
                for (float z : lz)
                    AppendBox(mesh.vertices, mesh.indices, glm::vec3(x, -0.48f, z), glm::vec3(0.06f, 0.22f, 0.07f), glm::vec4(0.36f, 0.20f, 0.12f, 1.0f));
            break;
        }
        case MeshPrimitive::EnemyKnight:
        {
            // 4. Iron Death-Knight (Plate-mail Swordsman)
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.14f, -0.45f, 0.0f), glm::vec3(0.10f, 0.30f, 0.10f), glm::vec4(0.52f, 0.54f, 0.58f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.14f, -0.45f, 0.0f), glm::vec3(0.10f, 0.30f, 0.10f), glm::vec4(0.52f, 0.54f, 0.58f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.08f, 0.0f), glm::vec3(0.27f, 0.29f, 0.17f), glm::vec4(0.62f, 0.65f, 0.70f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.52f, 0.0f), 0.14f, 0.16f, 0.28f, 10, glm::vec4(0.68f, 0.70f, 0.75f, 1.0f)); // Greathelm
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.34f, 0.26f, 0.38f), glm::vec3(0.03f, 0.42f, 0.07f), glm::vec4(0.85f, 0.88f, 0.94f, 1.0f)); // Broadsword blade
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.32f, 0.06f, 0.18f), glm::vec3(0.05f, 0.26f, 0.20f), glm::vec4(0.45f, 0.22f, 0.16f, 1.0f)); // Kite shield
            break;
        }
        case MeshPrimitive::EnemyHellKnight:
        {
            // 5. Hell Knight / Death Baron (Horned Crimson Plate + Runic Magic Sword)
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.17f, -0.45f, 0.0f), glm::vec3(0.12f, 0.32f, 0.12f), glm::vec4(0.42f, 0.16f, 0.14f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.17f, -0.45f, 0.0f), glm::vec3(0.12f, 0.32f, 0.12f), glm::vec4(0.42f, 0.16f, 0.14f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.12f, 0.0f), glm::vec3(0.34f, 0.33f, 0.20f), glm::vec4(0.58f, 0.22f, 0.16f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.58f, 0.0f), 0.16f, 0.18f, 0.30f, 10, glm::vec4(0.48f, 0.18f, 0.14f, 1.0f));
            // Demon horns
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(-0.18f, 0.76f, 0.04f), 0.0f, 0.06f, 0.28f, 8, glm::vec4(0.95f, 0.65f, 0.20f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3( 0.18f, 0.76f, 0.04f), 0.0f, 0.06f, 0.28f, 8, glm::vec4(0.95f, 0.65f, 0.20f, 1.0f));
            // Glowing Flamberge
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.38f, 0.32f, 0.42f), glm::vec3(0.04f, 0.48f, 0.08f), glm::vec4(1.0f, 0.45f, 0.15f, 1.0f));
            break;
        }
        case MeshPrimitive::EnemyOgre:
        {
            // 6. Chainsaw Ogre (Hulking Brute with Chainsaw + Grenade Launcher)
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.20f, -0.45f, 0.0f), glm::vec3(0.14f, 0.30f, 0.15f), glm::vec4(0.35f, 0.30f, 0.22f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.20f, -0.45f, 0.0f), glm::vec3(0.14f, 0.30f, 0.15f), glm::vec4(0.35f, 0.30f, 0.22f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.10f, 0.04f), 0.38f, 12, 14, glm::vec4(0.65f, 0.54f, 0.38f, 1.0f)); // Massive belly
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.54f, 0.06f), 0.20f, 10, 12, glm::vec4(0.60f, 0.48f, 0.34f, 1.0f));
            // Left hand Chainsaw
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.44f, 0.06f, 0.42f), glm::vec3(0.05f, 0.12f, 0.38f), glm::vec4(0.75f, 0.25f, 0.18f, 1.0f));
            // Right hand Grenade Launcher
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.44f, 0.12f, 0.38f), 0.09f, 0.10f, 0.56f, 10, glm::vec4(0.28f, 0.30f, 0.32f, 1.0f), true);
            break;
        }
        case MeshPrimitive::EnemyFiend:
        {
            // 7. Fiend / Demon (Hunched Horned Leaper with Long Scythe Claws)
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.05f, -0.05f), 0.36f, 12, 14, glm::vec4(0.68f, 0.26f, 0.22f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.24f, 0.32f), 0.24f, 10, 12, glm::vec4(0.74f, 0.30f, 0.24f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(-0.20f, 0.45f, 0.38f), 0.0f, 0.07f, 0.36f, 8, glm::vec4(0.90f, 0.82f, 0.65f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3( 0.20f, 0.45f, 0.38f), 0.0f, 0.07f, 0.36f, 8, glm::vec4(0.90f, 0.82f, 0.65f, 1.0f));
            // Scythe claws
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.38f, 0.0f, 0.44f), glm::vec3(0.06f, 0.18f, 0.28f), glm::vec4(0.88f, 0.78f, 0.62f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.38f, 0.0f, 0.44f), glm::vec3(0.06f, 0.18f, 0.28f), glm::vec4(0.88f, 0.78f, 0.62f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.18f, -0.45f, -0.10f), glm::vec3(0.10f, 0.28f, 0.12f), glm::vec4(0.58f, 0.20f, 0.18f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.18f, -0.45f, -0.10f), glm::vec3(0.10f, 0.28f, 0.12f), glm::vec4(0.58f, 0.20f, 0.18f, 1.0f));
            break;
        }
        case MeshPrimitive::EnemyScrag:
        {
            // 8. Scrag / Wizard (Hovering Serpent-Winged Acid Spitter)
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.28f, 0.05f), 0.28f, 12, 14, glm::vec4(0.72f, 0.82f, 0.55f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.18f, -0.10f), 0.20f, 0.02f, 0.68f, 10, glm::vec4(0.58f, 0.72f, 0.42f, 1.0f)); // Serpent tail
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.45f, 0.32f, -0.05f), glm::vec3(0.28f, 0.04f, 0.22f), glm::vec4(0.45f, 0.65f, 0.35f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.45f, 0.32f, -0.05f), glm::vec3(0.28f, 0.04f, 0.22f), glm::vec4(0.45f, 0.65f, 0.35f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.28f, 0.30f), 0.10f, 8, 10, glm::vec4(0.35f, 1.0f, 0.20f, 1.0f)); // Acid maw
            break;
        }
        case MeshPrimitive::EnemyVore:
        {
            // 9. Vore / Shalrath (Tri-Legged Spider Horror with Violet Spike Orbs)
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.15f, 0.0f), 0.32f, 12, 14, glm::vec4(0.62f, 0.38f, 0.68f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.52f, 0.08f), 0.18f, 10, 12, glm::vec4(0.75f, 0.48f, 0.82f, 1.0f));
            for (int k = 0; k < 3; ++k)
            {
                float ang = (k / 3.0f) * 2.0f * kPI + 0.52f;
                glm::vec3 legPos(std::cos(ang) * 0.32f, -0.35f, std::sin(ang) * 0.32f);
                AppendCylinder(mesh.vertices, mesh.indices, legPos, 0.08f, 0.04f, 0.72f, 8, glm::vec4(0.48f, 0.25f, 0.55f, 1.0f));
            }
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.42f, 0.38f), 0.12f, 10, 10, glm::vec4(0.95f, 0.25f, 1.0f, 1.0f)); // Violet orb
            break;
        }
        case MeshPrimitive::EnemyShambler:
        {
            // 10. Shambler (Massive Yeti-Like Lightning Horror Boss)
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.24f, -0.42f, 0.0f), glm::vec3(0.16f, 0.34f, 0.18f), glm::vec4(0.85f, 0.82f, 0.75f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.24f, -0.42f, 0.0f), glm::vec3(0.16f, 0.34f, 0.18f), glm::vec4(0.85f, 0.82f, 0.75f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.18f, 0.0f), 0.46f, 14, 16, glm::vec4(0.90f, 0.86f, 0.78f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.56f, 0.18f), 0.28f, 12, 14, glm::vec4(0.75f, 0.16f, 0.14f, 1.0f)); // Giant bloody maw
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.52f, 0.24f, 0.25f), glm::vec3(0.16f, 0.38f, 0.18f), glm::vec4(0.88f, 0.84f, 0.76f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.52f, 0.24f, 0.25f), glm::vec3(0.16f, 0.38f, 0.18f), glm::vec4(0.88f, 0.84f, 0.76f, 1.0f));
            // Bloody Claws + Lightning Arcs
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.52f, 0.58f, 0.35f), glm::vec3(0.12f, 0.12f, 0.12f), glm::vec4(0.45f, 0.92f, 1.0f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.52f, 0.58f, 0.35f), glm::vec3(0.12f, 0.12f, 0.12f), glm::vec4(0.45f, 0.92f, 1.0f, 1.0f));
            break;
        }

        // ==================== 5 QUAKE-INSPIRED 3D WEAPONS ====================
        case MeshPrimitive::WeaponAxe:
        {
            // 1. Runic Battle-Axe
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.05f, 0.0f), 0.035f, 0.04f, 0.85f, 10, glm::vec4(0.45f, 0.28f, 0.15f, 1.0f), true);
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.12f, 0.32f), glm::vec3(0.025f, 0.24f, 0.16f), glm::vec4(0.78f, 0.82f, 0.88f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.28f, 0.32f), glm::vec3(0.015f, 0.06f, 0.20f), glm::vec4(0.85f, 0.15f, 0.12f, 1.0f)); // bloodied edge
            break;
        }
        case MeshPrimitive::WeaponShotgun:
        {
            // 2. Double-Barrel Buckshot Shotgun
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.06f, -0.18f), glm::vec3(0.07f, 0.09f, 0.24f), glm::vec4(0.48f, 0.28f, 0.14f, 1.0f)); // Wooden stock
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(-0.045f, 0.02f, 0.24f), 0.038f, 0.038f, 0.68f, 12, glm::vec4(0.32f, 0.34f, 0.38f, 1.0f), true);
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3( 0.045f, 0.02f, 0.24f), 0.038f, 0.038f, 0.68f, 12, glm::vec4(0.32f, 0.34f, 0.38f, 1.0f), true);
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.03f, 0.18f), glm::vec3(0.08f, 0.04f, 0.18f), glm::vec4(0.52f, 0.32f, 0.16f, 1.0f)); // Pump grip
            break;
        }
        case MeshPrimitive::WeaponSuperNailgun:
        {
            // 3. Super Perforator Nailgun (Quad Barrels + Ammo Drum)
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.04f, -0.10f), glm::vec3(0.11f, 0.10f, 0.26f), glm::vec4(0.38f, 0.35f, 0.28f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.14f, 0.04f), 0.12f, 0.12f, 0.16f, 14, glm::vec4(0.55f, 0.46f, 0.24f, 1.0f)); // Nail drum
            float bx[2] = { -0.05f, 0.05f };
            float by[2] = { -0.01f, 0.07f };
            for (float x : bx)
                for (float y : by)
                    AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(x, y, 0.28f), 0.028f, 0.028f, 0.62f, 10, glm::vec4(0.58f, 0.62f, 0.66f, 1.0f), true);
            break;
        }
        case MeshPrimitive::WeaponRocketLauncher:
        {
            // 4. Ogre Rocket / Grenade Launcher
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.02f, 0.15f), 0.11f, 0.12f, 0.85f, 16, glm::vec4(0.46f, 0.36f, 0.22f, 1.0f), true);
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.02f, 0.56f), 0.13f, 0.13f, 0.10f, 16, glm::vec4(0.75f, 0.48f, 0.18f, 1.0f), true); // Muzzle collar
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.12f, 0.0f), glm::vec3(0.06f, 0.10f, 0.18f), glm::vec4(0.28f, 0.24f, 0.20f, 1.0f));
            break;
        }
        case MeshPrimitive::WeaponLightningGun:
        {
            // 5. Thunderbolt Lightning Shaft Gun
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.03f, -0.08f), glm::vec3(0.12f, 0.11f, 0.28f), glm::vec4(0.28f, 0.38f, 0.48f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.02f, 0.28f), 0.055f, 0.065f, 0.68f, 14, glm::vec4(0.35f, 0.85f, 1.0f, 1.0f), true);
            for (int r = 0; r < 3; ++r)
                AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.02f, 0.14f + r * 0.16f), 0.10f, 0.10f, 0.04f, 14, glm::vec4(0.85f, 0.95f, 1.0f, 1.0f), true);
            break;
        }

        // ==================== ATMOSPHERIC DECORATIONS & PROPS ====================
        case MeshPrimitive::PropTorch:
        {
            // Medieval Wall / Standing Torch with Glowing Flame
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.15f, 0.0f), 0.08f, 0.05f, 0.55f, 10, glm::vec4(0.32f, 0.22f, 0.14f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.14f, 0.0f), 0.13f, 0.09f, 0.10f, 10, glm::vec4(0.22f, 0.22f, 0.24f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.28f, 0.0f), 0.14f, 10, 12, glm::vec4(1.0f, 0.65f, 0.15f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.42f, 0.0f), 0.0f, 0.11f, 0.25f, 10, glm::vec4(1.0f, 0.88f, 0.25f, 1.0f));
            break;
        }
        case MeshPrimitive::PropBrazier:
        {
            // Iron Fire Brazier Stand
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.35f, 0.0f), 0.18f, 0.34f, 0.30f, 12, glm::vec4(0.24f, 0.22f, 0.20f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.05f, 0.0f), 0.36f, 0.20f, 0.32f, 14, glm::vec4(0.28f, 0.25f, 0.22f, 1.0f));
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.18f, 0.0f), 0.26f, 12, 14, glm::vec4(1.0f, 0.52f, 0.12f, 1.0f));
            break;
        }
        case MeshPrimitive::PropBarrel:
        {
            // Explosive Radioactive / Slipgate Fuel Barrel
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.0f, 0.0f), 0.36f, 0.36f, 0.92f, 16, glm::vec4(0.35f, 0.48f, 0.24f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.22f, 0.0f), 0.38f, 0.38f, 0.07f, 16, glm::vec4(0.85f, 0.65f, 0.15f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.22f, 0.0f), 0.38f, 0.38f, 0.07f, 16, glm::vec4(0.85f, 0.65f, 0.15f, 1.0f));
            break;
        }
        case MeshPrimitive::PropPillar:
        {
            // Gothic Carved Column with Base & Capital
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.44f, 0.0f), glm::vec3(0.42f, 0.08f, 0.42f), glm::vec4(0.52f, 0.48f, 0.42f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.0f, 0.0f), 0.30f, 0.32f, 0.84f, 12, glm::vec4(0.60f, 0.55f, 0.48f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.44f, 0.0f), glm::vec3(0.42f, 0.08f, 0.42f), glm::vec4(0.52f, 0.48f, 0.42f, 1.0f));
            break;
        }
        case MeshPrimitive::PropObelisk:
        {
            // Runic Obelisk Monument
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.42f, 0.0f), glm::vec3(0.44f, 0.10f, 0.44f), glm::vec4(0.38f, 0.34f, 0.30f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.04f, 0.0f), 0.18f, 0.34f, 0.82f, 4, glm::vec4(0.48f, 0.42f, 0.36f, 1.0f));
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.55f, 0.0f), 0.0f, 0.18f, 0.22f, 4, glm::vec4(0.95f, 0.35f, 0.18f, 1.0f));
            break;
        }
        case MeshPrimitive::PropCage:
        {
            // Hanging Iron Torture Cage + Chain
            AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.55f, 0.0f), 0.03f, 0.03f, 0.50f, 8, glm::vec4(0.25f, 0.25f, 0.28f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.28f, 0.0f), glm::vec3(0.32f, 0.04f, 0.32f), glm::vec4(0.28f, 0.26f, 0.24f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.28f, 0.0f), glm::vec3(0.32f, 0.04f, 0.32f), glm::vec4(0.28f, 0.26f, 0.24f, 1.0f));
            float cx[2] = { -0.28f, 0.28f };
            for (float x : cx)
                for (float z : cx)
                    AppendCylinder(mesh.vertices, mesh.indices, glm::vec3(x, 0.0f, z), 0.03f, 0.03f, 0.56f, 8, glm::vec4(0.32f, 0.30f, 0.28f, 1.0f));
            break;
        }
        case MeshPrimitive::PropCrate:
        {
            // Reinforced Supply Crate
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f), glm::vec3(0.46f), glm::vec4(0.68f, 0.52f, 0.34f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f), glm::vec3(0.48f, 0.48f, 0.10f), glm::vec4(0.38f, 0.36f, 0.34f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f), glm::vec3(0.10f, 0.48f, 0.48f), glm::vec4(0.38f, 0.36f, 0.34f, 1.0f));
            break;
        }
        case MeshPrimitive::PropAltar:
        {
            // Blood-Stained Sacrificial Altar
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.35f, 0.0f), glm::vec3(0.65f, 0.12f, 0.42f), glm::vec4(0.42f, 0.38f, 0.35f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(0.52f, 0.20f, 0.32f), glm::vec4(0.50f, 0.44f, 0.40f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.16f, 0.0f), glm::vec3(0.58f, 0.05f, 0.36f), glm::vec4(0.68f, 0.12f, 0.10f, 1.0f));
            break;
        }
        case MeshPrimitive::PropSlipgate:
        {
            // Quake Exit Slipgate Arch + Swirling Portal Plane
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, -0.45f, 0.0f), glm::vec3(0.65f, 0.08f, 0.45f), glm::vec4(0.38f, 0.36f, 0.34f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(-0.48f, 0.05f, 0.0f), glm::vec3(0.10f, 0.48f, 0.14f), glm::vec4(0.45f, 0.42f, 0.38f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3( 0.48f, 0.05f, 0.0f), glm::vec3(0.10f, 0.48f, 0.14f), glm::vec4(0.45f, 0.42f, 0.38f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.52f, 0.0f), glm::vec3(0.62f, 0.10f, 0.18f), glm::vec4(0.45f, 0.42f, 0.38f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.05f, 0.0f), glm::vec3(0.38f, 0.42f, 0.03f), glm::vec4(0.85f, 0.25f, 1.0f, 1.0f));
            break;
        }
        case MeshPrimitive::PropGib:
        {
            // Meaty Quake Gib Chunk
            AppendSphere(mesh.vertices, mesh.indices, glm::vec3(0.0f, 0.0f, 0.0f), 0.22f, 7, 8, glm::vec4(0.75f, 0.06f, 0.05f, 1.0f));
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.08f, 0.04f, 0.0f), glm::vec3(0.16f, 0.09f, 0.10f), glm::vec4(0.88f, 0.78f, 0.70f, 1.0f)); // Bone shard
            break;
        }

        default:
            AppendBox(mesh.vertices, mesh.indices, glm::vec3(0.0f), glm::vec3(0.5f), white);
            break;
        }

        UploadMeshBuffers(device, cmd, mesh);
        return mesh;
    }

    GPUMesh MeshFactory::LoadOBJ(nvrhi::IDevice* device, nvrhi::ICommandList* cmd, const std::string& objPath)
    {
        std::ifstream in(objPath);
        if (!in)
        {
            Log::Error("Failed to open OBJ file: " + objPath + ", falling back to Cube.");
            return CreatePrimitive(device, cmd, MeshPrimitive::Cube);
        }

        GPUMesh mesh;
        mesh.name = objPath;
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;

        std::string line;
        while (std::getline(in, line))
        {
            std::istringstream ss(line);
            std::string prefix;
            ss >> prefix;
            if (prefix == "v")
            {
                glm::vec3 p{ 0.0f };
                ss >> p.x >> p.y >> p.z;
                positions.push_back(p);
            }
            else if (prefix == "vn")
            {
                glm::vec3 n{ 0.0f, 1.0f, 0.0f };
                ss >> n.x >> n.y >> n.z;
                normals.push_back(n);
            }
            else if (prefix == "vt")
            {
                glm::vec2 uv{ 0.0f };
                ss >> uv.x >> uv.y;
                uvs.push_back(uv);
            }
            else if (prefix == "f")
            {
                std::vector<uint32_t> faceIndices;
                std::string token;
                while (ss >> token)
                {
                    int vi = 0, ti = 0, ni = 0;
                    size_t s1 = token.find('/');
                    if (s1 == std::string::npos)
                    {
                        vi = std::stoi(token);
                    }
                    else
                    {
                        vi = std::stoi(token.substr(0, s1));
                        size_t s2 = token.find('/', s1 + 1);
                        if (s2 == std::string::npos)
                        {
                            ti = std::stoi(token.substr(s1 + 1));
                        }
                        else
                        {
                            if (s2 > s1 + 1)
                                ti = std::stoi(token.substr(s1 + 1, s2 - s1 - 1));
                            if (s2 + 1 < token.size())
                                ni = std::stoi(token.substr(s2 + 1));
                        }
                    }

                    Vertex3D v{};
                    if (vi > 0 && static_cast<size_t>(vi) <= positions.size())
                        v.position = positions[vi - 1];
                    if (ti > 0 && static_cast<size_t>(ti) <= uvs.size())
                        v.uv = uvs[ti - 1];
                    if (ni > 0 && static_cast<size_t>(ni) <= normals.size())
                        v.normal = normals[ni - 1];
                    else
                        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                    v.color = glm::vec4(1.0f);

                    faceIndices.push_back(static_cast<uint32_t>(mesh.vertices.size()));
                    mesh.vertices.push_back(v);
                }

                for (size_t k = 1; k + 1 < faceIndices.size(); ++k)
                {
                    mesh.indices.push_back(faceIndices[0]);
                    mesh.indices.push_back(faceIndices[k]);
                    mesh.indices.push_back(faceIndices[k + 1]);
                }
            }
        }

        if (mesh.vertices.empty())
            return CreatePrimitive(device, cmd, MeshPrimitive::Cube);

        UploadMeshBuffers(device, cmd, mesh);
        return mesh;
    }

    void MeshFactory::UploadMeshBuffers(nvrhi::IDevice* device, nvrhi::ICommandList* cmd, GPUMesh& mesh)
    {
        mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
        if (!mesh.vertices.empty())
        {
            mesh.boundsMin = mesh.vertices[0].position;
            mesh.boundsMax = mesh.vertices[0].position;
            for (const auto& v : mesh.vertices)
            {
                mesh.boundsMin = glm::min(mesh.boundsMin, v.position);
                mesh.boundsMax = glm::max(mesh.boundsMax, v.position);
            }
        }

        nvrhi::BufferDesc vbDesc;
        vbDesc.byteSize = mesh.vertices.size() * sizeof(Vertex3D);
        vbDesc.isVertexBuffer = true;
        vbDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
        vbDesc.keepInitialState = true;
        vbDesc.debugName = "MeshVB_" + mesh.name;
        mesh.vertexBuffer = device->createBuffer(vbDesc);

        nvrhi::BufferDesc ibDesc;
        ibDesc.byteSize = mesh.indices.size() * sizeof(uint32_t);
        ibDesc.isIndexBuffer = true;
        ibDesc.initialState = nvrhi::ResourceStates::IndexBuffer;
        ibDesc.keepInitialState = true;
        ibDesc.debugName = "MeshIB_" + mesh.name;
        mesh.indexBuffer = device->createBuffer(ibDesc);

        cmd->writeBuffer(mesh.vertexBuffer, mesh.vertices.data(), vbDesc.byteSize);
        cmd->writeBuffer(mesh.indexBuffer, mesh.indices.data(), ibDesc.byteSize);
    }
}
