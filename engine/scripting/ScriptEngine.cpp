#include "ScriptEngine.h"
#include "../project/SceneTemplates.h"
#include "../physics/PhysicsSystem.h"
#include "../core/Window.h"
#include "../core/InputMap.h"
#include "../core/Log.h"
#include "../animation/TweenSystem.h"
#include "../audio/AudioEngine.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>

namespace Hargon
{
    static ScriptEngine* GetScriptEngine(lua_State* L)
    {
        lua_getfield(L, LUA_REGISTRYINDEX, "HargonScriptEnginePtr");
        auto* ptr = static_cast<ScriptEngine*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        return ptr;
    }

    // --- Lua C API Bindings ---

    static int Lua_Log(lua_State* L)
    {
        const char* msg = luaL_checkstring(L, 1);
        Log::Lua(msg ? msg : "");
        return 0;
    }

    static int Lua_GetPosition(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        if (se && se->GetScene())
        {
            if (Entity* e = se->GetScene()->GetEntity(id))
            {
                lua_pushnumber(L, e->transform.position.x);
                lua_pushnumber(L, e->transform.position.y);
                lua_pushnumber(L, e->transform.position.z);
                return 3;
            }
        }
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 3;
    }

    static int Lua_SetPosition(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        float x = static_cast<float>(luaL_checknumber(L, 2));
        float y = static_cast<float>(luaL_checknumber(L, 3));
        float z = static_cast<float>(luaL_checknumber(L, 4));
        if (se && se->GetScene())
        {
            if (Entity* e = se->GetScene()->GetEntity(id))
                e->transform.position = glm::vec3(x, y, z);
        }
        return 0;
    }

    static int Lua_GetRotation(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        if (se && se->GetScene())
        {
            if (Entity* e = se->GetScene()->GetEntity(id))
            {
                lua_pushnumber(L, e->transform.rotation.x);
                lua_pushnumber(L, e->transform.rotation.y);
                lua_pushnumber(L, e->transform.rotation.z);
                return 3;
            }
        }
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 3;
    }

    static int Lua_SetRotation(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        float rx = static_cast<float>(luaL_checknumber(L, 2));
        float ry = static_cast<float>(luaL_checknumber(L, 3));
        float rz = static_cast<float>(luaL_checknumber(L, 4));
        if (se && se->GetScene())
        {
            if (Entity* e = se->GetScene()->GetEntity(id))
                e->transform.rotation = glm::vec3(rx, ry, rz);
        }
        return 0;
    }

    static int Lua_GetForward(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        if (se && se->GetScene())
        {
            if (Entity* e = se->GetScene()->GetEntity(id))
            {
                glm::vec3 f = e->transform.GetForward();
                lua_pushnumber(L, f.x);
                lua_pushnumber(L, f.y);
                lua_pushnumber(L, f.z);
                return 3;
            }
        }
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 1.0);
        return 3;
    }

    static int Lua_GetVelocity(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        if (se && se->GetScene())
        {
            if (Entity* e = se->GetScene()->GetEntity(id))
            {
                lua_pushnumber(L, e->rigidBody.velocity.x);
                lua_pushnumber(L, e->rigidBody.velocity.y);
                lua_pushnumber(L, e->rigidBody.velocity.z);
                return 3;
            }
        }
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 3;
    }

    static int Lua_SetVelocity(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        float vx = static_cast<float>(luaL_checknumber(L, 2));
        float vy = static_cast<float>(luaL_checknumber(L, 3));
        float vz = static_cast<float>(luaL_checknumber(L, 4));
        if (se && se->GetScene())
        {
            if (Entity* e = se->GetScene()->GetEntity(id))
                e->rigidBody.velocity = glm::vec3(vx, vy, vz);
        }
        return 0;
    }

    static int Lua_SetEmissive(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        float r = static_cast<float>(luaL_checknumber(L, 2));
        float g = static_cast<float>(luaL_checknumber(L, 3));
        float b = static_cast<float>(luaL_checknumber(L, 4));
        float s = static_cast<float>(luaL_checknumber(L, 5));
        if (se && se->GetScene())
        {
            if (Entity* e = se->GetScene()->GetEntity(id))
            {
                e->material.emissiveColor = glm::vec3(r, g, b);
                e->material.emissiveStrength = s;
            }
        }
        return 0;
    }

    static int Lua_IsKeyDown(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* key = luaL_checkstring(L, 1);
        bool down = (se && se->GetWindow()) ? se->GetWindow()->IsKeyDownByName(key) : false;
        lua_pushboolean(L, down ? 1 : 0);
        return 1;
    }

    static int Lua_IsKeyPressed(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* key = luaL_checkstring(L, 1);
        bool pressed = (se && se->GetWindow()) ? se->GetWindow()->IsKeyPressedByName(key) : false;
        lua_pushboolean(L, pressed ? 1 : 0);
        return 1;
    }

    static int Lua_FindEntity(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* name = luaL_checkstring(L, 1);
        if (se && se->GetScene() && name)
        {
            if (Entity* e = se->GetScene()->FindEntityByName(name))
            {
                lua_pushinteger(L, e->id);
                return 1;
            }
        }
        lua_pushinteger(L, 0);
        return 1;
    }

    static int Lua_FindNearestByTag(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        float x = static_cast<float>(luaL_checknumber(L, 1));
        float y = static_cast<float>(luaL_checknumber(L, 2));
        float z = static_cast<float>(luaL_checknumber(L, 3));
        const char* tag = luaL_checkstring(L, 4);

        EntityID bestId = kInvalidEntity;
        float bestDist = 999999.0f;

        if (se && se->GetScene() && tag)
        {
            glm::vec3 origin(x, y, z);
            for (const auto& e : se->GetScene()->GetEntities())
            {
                if (e.alive && e.tag.tag == tag)
                {
                    float d = glm::length(e.transform.position - origin);
                    if (d < bestDist)
                    {
                        bestDist = d;
                        bestId = e.id;
                    }
                }
            }
        }
        lua_pushinteger(L, bestId);
        lua_pushnumber(L, bestDist);
        return 2;
    }

    static int Lua_GetTag(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        if (se && se->GetScene())
        {
            if (Entity* e = se->GetScene()->GetEntity(id))
            {
                lua_pushstring(L, e->tag.tag.c_str());
                return 1;
            }
        }
        lua_pushstring(L, "");
        return 1;
    }

    static int Lua_GetGameplayStats(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        if (se && se->GetScene())
        {
            if (Entity* e = se->GetScene()->GetEntity(id))
            {
                lua_pushnumber(L, e->gameplay.health);
                lua_pushnumber(L, e->gameplay.maxHealth);
                lua_pushnumber(L, e->gameplay.moveSpeed);
                lua_pushnumber(L, e->gameplay.fireRate);
                return 4;
            }
        }
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 100.0);
        lua_pushnumber(L, 10.0);
        lua_pushnumber(L, 0.5);
        return 4;
    }

    static int Lua_FireProjectile(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID ownerId = static_cast<EntityID>(luaL_checkinteger(L, 1));
        float px = static_cast<float>(luaL_checknumber(L, 2));
        float py = static_cast<float>(luaL_checknumber(L, 3));
        float pz = static_cast<float>(luaL_checknumber(L, 4));
        float dx = static_cast<float>(luaL_checknumber(L, 5));
        float dy = static_cast<float>(luaL_checknumber(L, 6));
        float dz = static_cast<float>(luaL_checknumber(L, 7));
        bool isEnemy = lua_toboolean(L, 8) != 0;
        float speed = static_cast<float>(luaL_optnumber(L, 9, 38.0));
        float dmg = static_cast<float>(luaL_optnumber(L, 10, 25.0));

        if (se && se->GetScene())
        {
            Entity& proj = SceneTemplates::SpawnProjectile(
                *se->GetScene(),
                glm::vec3(px, py, pz),
                glm::vec3(dx, dy, dz),
                ownerId,
                isEnemy,
                speed,
                dmg);
            lua_pushinteger(L, proj.id);
            return 1;
        }
        lua_pushinteger(L, 0);
        return 1;
    }

    static int Lua_PlaySound(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* name = luaL_checkstring(L, 1);
        float vol = static_cast<float>(luaL_optnumber(L, 2, 0.8));
        float pitch = static_cast<float>(luaL_optnumber(L, 3, 1.0));
        if (se && se->GetAudio() && name)
            se->GetAudio()->PlaySound2D(name, vol, pitch);
        return 0;
    }

    static int Lua_SpawnParticles(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        float x = static_cast<float>(luaL_checknumber(L, 1));
        float y = static_cast<float>(luaL_checknumber(L, 2));
        float z = static_cast<float>(luaL_checknumber(L, 3));
        float r = static_cast<float>(luaL_checknumber(L, 4));
        float g = static_cast<float>(luaL_checknumber(L, 5));
        float b = static_cast<float>(luaL_checknumber(L, 6));
        int count = static_cast<int>(luaL_optinteger(L, 7, 16));
        float speed = static_cast<float>(luaL_optnumber(L, 8, 8.0));
        if (se)
            se->TriggerParticles(glm::vec3(x, y, z), glm::vec3(r, g, b), count, speed);
        return 0;
    }

    static int Lua_AddScore(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        int pts = static_cast<int>(luaL_checkinteger(L, 1));
        if (se && se->GetScene())
            se->GetScene()->score += pts;
        return 0;
    }

    static int Lua_DestroyEntity(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        if (se && se->GetScene())
            se->GetScene()->DestroyEntity(id);
        return 0;
    }

    static int Lua_TweenPosition(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        float tx = static_cast<float>(luaL_checknumber(L, 2));
        float ty = static_cast<float>(luaL_checknumber(L, 3));
        float tz = static_cast<float>(luaL_checknumber(L, 4));
        float dur = static_cast<float>(luaL_optnumber(L, 5, 0.5));
        if (se && se->GetScene())
        {
            TweenSystem::Get().TweenEntityVec3(
                *se->GetScene(), id, TweenProperty::Position,
                glm::vec3(tx, ty, tz), dur, TweenTransition::Cubic, TweenEase::Out);
        }
        return 0;
    }

    static int Lua_TweenScale(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        float sx = static_cast<float>(luaL_checknumber(L, 2));
        float sy = static_cast<float>(luaL_checknumber(L, 3));
        float sz = static_cast<float>(luaL_checknumber(L, 4));
        float dur = static_cast<float>(luaL_optnumber(L, 5, 0.5));
        if (se && se->GetScene())
        {
            TweenSystem::Get().TweenEntityVec3(
                *se->GetScene(), id, TweenProperty::Scale,
                glm::vec3(sx, sy, sz), dur, TweenTransition::Elastic, TweenEase::Out);
        }
        return 0;
    }

    static int Lua_EmitSignal(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* sig = luaL_checkstring(L, 1);
        EntityID id = static_cast<EntityID>(luaL_optinteger(L, 2, 0));
        const char* payload = luaL_optstring(L, 3, "");
        if (se && se->GetScene() && sig)
            se->GetScene()->EmitSignal(sig, id, payload ? payload : "");
        return 0;
    }

    static int Lua_IsActionPressed(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* act = luaL_checkstring(L, 1);
        bool down = false;
        if (se && se->GetWindow() && act)
            down = InputMap::Get().IsActionPressed(act, se->GetWindow());
        lua_pushboolean(L, down ? 1 : 0);
        return 1;
    }

    static int Lua_GetGroupCount(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* grp = luaL_checkstring(L, 1);
        int cnt = 0;
        if (se && se->GetScene() && grp)
            cnt = static_cast<int>(se->GetScene()->GetEntitiesInGroup(grp).size());
        lua_pushinteger(L, cnt);
        return 1;
    }

    static int Lua_Raycast(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        float ox = static_cast<float>(luaL_checknumber(L, 1));
        float oy = static_cast<float>(luaL_checknumber(L, 2));
        float oz = static_cast<float>(luaL_checknumber(L, 3));
        float dx = static_cast<float>(luaL_checknumber(L, 4));
        float dy = static_cast<float>(luaL_checknumber(L, 5));
        float dz = static_cast<float>(luaL_checknumber(L, 6));
        float maxDist = static_cast<float>(luaL_optnumber(L, 7, 100.0));
        EntityID ignoreId = static_cast<EntityID>(luaL_optinteger(L, 8, kInvalidEntity));

        PhysicsRaycastHit hit;
        if (se && se->GetScene())
            hit = PhysicsSystem::Raycast(*se->GetScene(), glm::vec3(ox, oy, oz), glm::vec3(dx, dy, dz), maxDist, 0xFFFFFFFFu, ignoreId);

        lua_pushboolean(L, hit.hit ? 1 : 0);
        lua_pushinteger(L, hit.entityId);
        lua_pushnumber(L, hit.distance);
        lua_pushnumber(L, hit.point.x);
        lua_pushnumber(L, hit.point.y);
        lua_pushnumber(L, hit.point.z);
        return 6;
    }

    static int Lua_RaycastNode(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        if (se && se->GetScene())
        {
            PhysicsSystem::UpdateRayCastNodes(*se->GetScene());
            if (const Entity* e = se->GetScene()->GetEntity(id))
            {
                lua_pushboolean(L, e->raycast.isColliding ? 1 : 0);
                lua_pushinteger(L, e->raycast.hitEntity);
                lua_pushnumber(L, e->raycast.hitDistance);
                lua_pushnumber(L, e->raycast.hitPoint.x);
                lua_pushnumber(L, e->raycast.hitPoint.y);
                lua_pushnumber(L, e->raycast.hitPoint.z);
                lua_pushnumber(L, e->raycast.hitNormal.x);
                lua_pushnumber(L, e->raycast.hitNormal.y);
                lua_pushnumber(L, e->raycast.hitNormal.z);
                return 9;
            }
        }
        lua_pushboolean(L, 0);
        lua_pushinteger(L, kInvalidEntity);
        for (int i = 0; i < 7; ++i) lua_pushnumber(L, 0.0);
        return 9;
    }

    static int Lua_FindPath(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        float sx = static_cast<float>(luaL_checknumber(L, 1));
        float sy = static_cast<float>(luaL_checknumber(L, 2));
        float sz = static_cast<float>(luaL_checknumber(L, 3));
        float gx = static_cast<float>(luaL_checknumber(L, 4));
        float gy = static_cast<float>(luaL_checknumber(L, 5));
        float gz = static_cast<float>(luaL_checknumber(L, 6));
        float cellSize = static_cast<float>(luaL_optnumber(L, 7, 1.5));

        lua_newtable(L);
        if (se && se->GetScene())
        {
            auto pts = PhysicsSystem::FindPathGrid(*se->GetScene(), glm::vec3(sx, sy, sz), glm::vec3(gx, gy, gz), cellSize);
            for (size_t i = 0; i < pts.size(); ++i)
            {
                lua_newtable(L);
                lua_pushnumber(L, pts[i].x); lua_rawseti(L, -2, 1);
                lua_pushnumber(L, pts[i].y); lua_rawseti(L, -2, 2);
                lua_pushnumber(L, pts[i].z); lua_rawseti(L, -2, 3);
                lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
            }
        }
        return 1;
    }

    static int Lua_IsOnFloor(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        EntityID id = static_cast<EntityID>(luaL_checkinteger(L, 1));
        bool onFloor = false;
        if (se && se->GetScene())
        {
            if (const Entity* e = se->GetScene()->GetEntity(id))
                onFloor = e->rigidBody.isOnFloor;
        }
        lua_pushboolean(L, onFloor ? 1 : 0);
        return 1;
    }

    static int Lua_CreateTimer(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* name = luaL_checkstring(L, 1);
        float secs = static_cast<float>(luaL_checknumber(L, 2));
        if (se && name)
            se->CreateTimer(name, secs);
        return 0;
    }

    static int Lua_IsTimerReady(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* name = luaL_checkstring(L, 1);
        bool ready = (se && name) ? se->IsTimerReady(name) : true;
        lua_pushboolean(L, ready ? 1 : 0);
        return 1;
    }

    static int Lua_GetTimerLeft(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* name = luaL_checkstring(L, 1);
        float left = (se && name) ? se->GetTimerLeft(name) : 0.0f;
        lua_pushnumber(L, left);
        return 1;
    }

    static int Lua_PlaySound3D(lua_State* L)
    {
        auto* se = GetScriptEngine(L);
        const char* name = luaL_checkstring(L, 1);
        float x = static_cast<float>(luaL_checknumber(L, 2));
        float y = static_cast<float>(luaL_checknumber(L, 3));
        float z = static_cast<float>(luaL_checknumber(L, 4));
        float vol = static_cast<float>(luaL_optnumber(L, 5, 0.85));
        float pitch = static_cast<float>(luaL_optnumber(L, 6, 1.0));
        int atten = static_cast<int>(luaL_optinteger(L, 7, 0));
        float unitSize = static_cast<float>(luaL_optnumber(L, 8, 10.0));
        float maxDist = static_cast<float>(luaL_optnumber(L, 9, 65.0));
        float pitchRand = static_cast<float>(luaL_optnumber(L, 10, 0.04));
        if (se && se->GetAudio() && name)
            se->GetAudio()->PlaySound3D(name, glm::vec3(x, y, z), vol, pitch, atten, unitSize, maxDist, pitchRand);
        return 0;
    }

    ScriptEngine::~ScriptEngine()
    {
        Shutdown();
    }

    bool ScriptEngine::Init(Scene* scene, Window* window, AudioEngine* audio)
    {
        m_Scene = scene;
        m_Window = window;
        m_Audio = audio;

        EnsureDefaultLuaScripts("assets/scripts");

        m_L = luaL_newstate();
        if (!m_L)
        {
            Log::Error("Failed to create Lua 5.4 state!");
            return false;
        }

        luaL_openlibs(m_L);

        lua_pushlightuserdata(m_L, this);
        lua_setfield(m_L, LUA_REGISTRYINDEX, "HargonScriptEnginePtr");

        RegisterBindings();
        Log::Lua("Lua 5.4 Scripting Engine initialized.");
        return true;
    }

    void ScriptEngine::Shutdown()
    {
        if (m_L)
        {
            lua_close(m_L);
            m_L = nullptr;
        }
    }

    void ScriptEngine::RegisterBindings()
    {
        lua_newtable(m_L);

        auto bind = [this](const char* name, lua_CFunction fn) {
            lua_pushcfunction(m_L, fn);
            lua_setfield(m_L, -2, name);
        };

        bind("log",                 Lua_Log);
        bind("get_position",        Lua_GetPosition);
        bind("set_position",        Lua_SetPosition);
        bind("get_rotation",        Lua_GetRotation);
        bind("set_rotation",        Lua_SetRotation);
        bind("get_forward",         Lua_GetForward);
        bind("get_velocity",        Lua_GetVelocity);
        bind("set_velocity",        Lua_SetVelocity);
        bind("set_emissive",        Lua_SetEmissive);
        bind("is_key_down",         Lua_IsKeyDown);
        bind("is_key_pressed",      Lua_IsKeyPressed);
        bind("find_entity",         Lua_FindEntity);
        bind("find_nearest_by_tag", Lua_FindNearestByTag);
        bind("get_tag",             Lua_GetTag);
        bind("get_gameplay_stats",  Lua_GetGameplayStats);
        bind("fire_projectile",     Lua_FireProjectile);
        bind("play_sound",          Lua_PlaySound);
        bind("play_sound_3d",       Lua_PlaySound3D);
        bind("spawn_particles",     Lua_SpawnParticles);
        bind("add_score",           Lua_AddScore);
        bind("destroy_entity",      Lua_DestroyEntity);
        bind("tween_position",      Lua_TweenPosition);
        bind("tween_scale",         Lua_TweenScale);
        bind("emit_signal",         Lua_EmitSignal);
        bind("is_action_pressed",   Lua_IsActionPressed);
        bind("get_group_count",     Lua_GetGroupCount);
        bind("raycast",             Lua_Raycast);
        bind("raycast_node",        Lua_RaycastNode);
        bind("find_path",           Lua_FindPath);
        bind("is_on_floor",         Lua_IsOnFloor);
        bind("create_timer",        Lua_CreateTimer);
        bind("is_timer_ready",      Lua_IsTimerReady);
        bind("get_timer_left",      Lua_GetTimerLeft);

        lua_setglobal(m_L, "Engine");
    }

    void ScriptEngine::TriggerParticles(const glm::vec3& pos, const glm::vec3& color, int count, float speed)
    {
        if (m_ParticleCallback)
            m_ParticleCallback(pos, color, count, speed);
    }

    bool ScriptEngine::LoadAndCallEntityFunction(Entity& entity, const char* funcName, float arg1, bool hasArg1, EntityID argEntity)
    {
        if (!m_L || entity.luaScript.scriptPath.empty())
            return false;

        std::string envName = "ScriptEnv_" + std::to_string(entity.id);

        if (!entity.luaScript.initialized)
        {
            std::string scriptFile = entity.luaScript.scriptPath;
            if (!std::filesystem::exists(scriptFile))
            {
                std::string fallback = "assets/scripts/" + std::filesystem::path(scriptFile).filename().string();
                if (std::filesystem::exists(fallback))
                    scriptFile = fallback;
                else
                    return false;
            }

            // Create an isolated environment table inheriting _G
            lua_newtable(m_L);
            lua_newtable(m_L);
            lua_getglobal(m_L, "_G");
            lua_setfield(m_L, -2, "__index");
            lua_setmetatable(m_L, -2);

            if (luaL_loadfile(m_L, scriptFile.c_str()) != LUA_OK)
            {
                const char* err = lua_tostring(m_L, -1);
                Log::Error(std::string("[Lua Load Error] ") + (err ? err : ""));
                lua_pop(m_L, 2);
                return false;
            }

            // Set _ENV of loaded chunk to our environment table
            lua_pushvalue(m_L, -2);
            lua_setupvalue(m_L, -2, 1);

            if (lua_pcall(m_L, 0, 0, 0) != LUA_OK)
            {
                const char* err = lua_tostring(m_L, -1);
                Log::Error(std::string("[Lua Exec Error] ") + (err ? err : ""));
                lua_pop(m_L, 2);
                return false;
            }

            // Save environment table in registry
            lua_setfield(m_L, LUA_REGISTRYINDEX, envName.c_str());
            entity.luaScript.initialized = true;

            // Call on_init(id) if defined
            lua_getfield(m_L, LUA_REGISTRYINDEX, envName.c_str());
            lua_getfield(m_L, -1, "on_init");
            if (lua_isfunction(m_L, -1))
            {
                lua_pushinteger(m_L, entity.id);
                if (lua_pcall(m_L, 1, 0, 0) != LUA_OK)
                {
                    const char* err = lua_tostring(m_L, -1);
                    Log::Error(std::string("[Lua on_init Error] ") + (err ? err : ""));
                    lua_pop(m_L, 1);
                }
            }
            else
            {
                lua_pop(m_L, 1);
            }
            lua_pop(m_L, 1);
        }

        lua_getfield(m_L, LUA_REGISTRYINDEX, envName.c_str());
        if (!lua_istable(m_L, -1))
        {
            lua_pop(m_L, 1);
            return false;
        }

        lua_getfield(m_L, -1, funcName);
        if (!lua_isfunction(m_L, -1))
        {
            lua_pop(m_L, 2);
            return false;
        }

        lua_pushinteger(m_L, entity.id);
        int nargs = 1;
        if (hasArg1)
        {
            lua_pushnumber(m_L, arg1);
            ++nargs;
        }
        if (argEntity != kInvalidEntity)
        {
            lua_pushinteger(m_L, argEntity);
            ++nargs;
        }

        if (lua_pcall(m_L, nargs, 0, 0) != LUA_OK)
        {
            const char* err = lua_tostring(m_L, -1);
            Log::Error(std::string("[Lua ") + funcName + " Error] " + (err ? err : ""));
            lua_pop(m_L, 2);
            return false;
        }

        lua_pop(m_L, 1);
        return true;
    }

    void ScriptEngine::CreateTimer(const std::string& name, float seconds)
    {
        m_Timers[name] = std::max(0.0f, seconds);
    }

    bool ScriptEngine::IsTimerReady(const std::string& name) const
    {
        auto it = m_Timers.find(name);
        return it == m_Timers.end() || it->second <= 0.0f;
    }

    float ScriptEngine::GetTimerLeft(const std::string& name) const
    {
        auto it = m_Timers.find(name);
        return (it != m_Timers.end()) ? std::max(0.0f, it->second) : 0.0f;
    }

    void ScriptEngine::UpdateScripts(Scene& scene, float dt)
    {
        m_Scene = &scene;
        for (auto& [k, rem] : m_Timers)
        {
            if (rem > 0.0f)
                rem = std::max(0.0f, rem - dt);
        }

        size_t count = scene.GetEntities().size();
        for (size_t i = 0; i < count; ++i)
        {
            Entity& e = scene.GetEntities()[i];
            if (!scene.ShouldProcessEntity(e, false) || !e.luaScript.enabled || e.luaScript.scriptPath.empty())
                continue;

            LoadAndCallEntityFunction(e, "on_update", dt, true, kInvalidEntity);
        }
    }

    void ScriptEngine::DispatchCollision(Scene& scene, EntityID a, EntityID b)
    {
        m_Scene = &scene;
        if (Entity* ea = scene.GetEntity(a))
        {
            if (ea->alive && ea->luaScript.enabled && !ea->luaScript.scriptPath.empty())
                LoadAndCallEntityFunction(*ea, "on_collision", 0.0f, false, b);
        }
        if (Entity* eb = scene.GetEntity(b))
        {
            if (eb->alive && eb->luaScript.enabled && !eb->luaScript.scriptPath.empty())
                LoadAndCallEntityFunction(*eb, "on_collision", 0.0f, false, a);
        }
    }

    void ScriptEngine::DispatchTrigger(Scene& scene, EntityID a, EntityID b, bool entered)
    {
        m_Scene = &scene;
        const char* fn = entered ? "on_trigger_enter" : "on_trigger_exit";
        if (Entity* ea = scene.GetEntity(a))
        {
            if (ea->alive && ea->luaScript.enabled && !ea->luaScript.scriptPath.empty())
                LoadAndCallEntityFunction(*ea, fn, 0.0f, false, b);
        }
        if (Entity* eb = scene.GetEntity(b))
        {
            if (eb->alive && eb->luaScript.enabled && !eb->luaScript.scriptPath.empty())
                LoadAndCallEntityFunction(*eb, fn, 0.0f, false, a);
        }
    }

    bool ScriptEngine::ReloadAllScripts(Scene& scene)
    {
        for (auto& e : scene.GetEntities())
        {
            e.luaScript.initialized = false;
        }
        Log::Lua("Hot-reloaded all entity Lua scripts.");
        return true;
    }

    bool ScriptEngine::ExecuteSnippet(const std::string& luaCode)
    {
        if (!m_L) return false;
        if (luaL_dostring(m_L, luaCode.c_str()) != LUA_OK)
        {
            const char* err = lua_tostring(m_L, -1);
            Log::Error(std::string("[Lua Console Error] ") + (err ? err : ""));
            lua_pop(m_L, 1);
            return false;
        }
        return true;
    }

    static void WriteScriptIfMissing(const std::string& path, const char* content)
    {
        if (std::filesystem::exists(path))
            return;
        std::ofstream out(path, std::ios::out | std::ios::trunc);
        if (out)
            out << content;
    }

    void ScriptEngine::EnsureDefaultLuaScripts(const std::string& scriptDir)
    {
        std::filesystem::create_directories(scriptDir);

        // 1. player_rover.lua
        WriteScriptIfMissing(scriptDir + "/player_rover.lua", R"LUA(-- Player Hover-Rover Controller Script (Hargon Engine Lua 5.4)
local cooldown = 0.0
local bob_time = 0.0

function on_init(id)
    Engine.log("PlayerRover Lua script initialized (Entity " .. tostring(id) .. ")")
end

function on_update(id, dt)
    bob_time = bob_time + dt
    cooldown = math.max(0.0, cooldown - dt)

    local hp, max_hp, speed, fire_rate = Engine.get_gameplay_stats(id)
    local rx, ry, rz = Engine.get_rotation(id)

    -- Steering / Yaw rotation (A/D or Left/Right)
    local turn_speed = 135.0
    if Engine.is_key_down("A") or Engine.is_key_down("LEFT") then
        ry = ry - turn_speed * dt
    end
    if Engine.is_key_down("D") or Engine.is_key_down("RIGHT") then
        ry = ry + turn_speed * dt
    end
    Engine.set_rotation(id, rx, ry, rz)

    -- Forward / Reverse thrust (W/S or Up/Down) + SHIFT Turbo Boost
    local fx, fy, fz = Engine.get_forward(id)
    local thrust = 0.0
    if Engine.is_key_down("W") or Engine.is_key_down("UP") then
        thrust = 1.0
    elseif Engine.is_key_down("S") or Engine.is_key_down("DOWN") then
        thrust = -0.75
    end

    local boost = 1.0
    if Engine.is_key_down("SHIFT") then
        boost = 1.65
    end

    if math.abs(thrust) > 0.01 then
        local vx = fx * speed * thrust * boost
        local vz = fz * speed * thrust * boost
        Engine.set_velocity(id, vx, 0.0, vz)
    end

    -- Hover oscillation
    local px, py, pz = Engine.get_position(id)
    py = 0.65 + math.sin(bob_time * 5.0) * 0.06
    Engine.set_position(id, px, py, pz)

    -- Dual Plasma Cannons (SPACE or F)
    if (Engine.is_key_down("SPACE") or Engine.is_key_down("F")) and cooldown <= 0.0 then
        cooldown = fire_rate
        local spawn_x = px + fx * 1.45
        local spawn_y = py + 0.25
        local spawn_z = pz + fz * 1.45
        Engine.fire_projectile(id, spawn_x, spawn_y, spawn_z, fx, 0.0, fz, false, 42.0, 25.0)
        Engine.play_sound("laser", 0.75, 1.0)
        Engine.spawn_particles(spawn_x, spawn_y, spawn_z, 0.2, 0.95, 1.0, 6, 6.0)
    end
end
)LUA");

        // 2. enemy_drone.lua
        WriteScriptIfMissing(scriptDir + "/enemy_drone.lua", R"LUA(-- Enemy Seeker Drone AI Script (Hargon Engine Lua 5.4)
local timer = 0.0
local shoot_cd = 1.6

function on_init(id)
    timer = (id % 7) * 0.45
    shoot_cd = 1.2 + (id % 4) * 0.35
end

function on_update(id, dt)
    timer = timer + dt
    shoot_cd = shoot_cd - dt

    local px, py, pz = Engine.get_position(id)
    local hp, max_hp, speed, fire_rate = Engine.get_gameplay_stats(id)

    -- Find Player or Core to attack
    local target_id = Engine.find_entity("PlayerRover")
    local core_id = Engine.find_entity("StarforgeCore")

    local tx, ty, tz = 0.0, 1.0, 0.0
    if target_id ~= 0 then
        local rpx, rpy, rpz = Engine.get_position(target_id)
        local dist_player = math.sqrt((rpx - px)^2 + (rpz - pz)^2)
        if dist_player < 20.0 or core_id == 0 then
            tx, ty, tz = rpx, rpy, rpz
        else
            tx, ty, tz = Engine.get_position(core_id)
        end
    elseif core_id ~= 0 then
        tx, ty, tz = Engine.get_position(core_id)
    end

    local dx = tx - px
    local dz = tz - pz
    local dist = math.sqrt(dx * dx + dz * dz)

    if dist > 0.1 then
        local nx = dx / dist
        local nz = dz / dist

        -- Face target
        local yaw = math.atan(nx, nz) * (180.0 / 3.14159265)
        local roll = math.sin(timer * 4.0) * 12.0
        Engine.set_rotation(id, 0.0, yaw, roll)

        -- Approach and strafe
        local strafe_x = -nz * math.sin(timer * 2.5) * 0.45
        local strafe_z =  nx * math.sin(timer * 2.5) * 0.45
        local approach = (dist > 5.5) and 1.0 or 0.25

        local vx = (nx * approach + strafe_x) * speed
        local vz = (nz * approach + strafe_z) * speed
        Engine.set_velocity(id, vx, 0.0, vz)

        -- Fire enemy plasma bolt when in range
        if shoot_cd <= 0.0 and dist < 24.0 then
            shoot_cd = fire_rate
            Engine.fire_projectile(id, px + nx * 1.2, py, pz + nz * 1.2, nx, 0.0, nz, true, 24.0, 10.0)
        end
    end

    -- Hover height
    py = 0.95 + math.sin(timer * 4.2) * 0.15
    Engine.set_position(id, px, py, pz)
end
)LUA");

        // 3. crystal_pickup.lua
        WriteScriptIfMissing(scriptDir + "/crystal_pickup.lua", R"LUA(-- Energy Crystal Rotation & Magnetic Pull Script (Hargon Engine Lua 5.4)
local t = 0.0

function on_init(id)
    t = (id % 10) * 0.5
end

function on_update(id, dt)
    t = t + dt
    local rx, ry, rz = Engine.get_rotation(id)
    ry = ry + 95.0 * dt
    Engine.set_rotation(id, 12.0 * math.sin(t * 2.0), ry, 0.0)

    local px, py, pz = Engine.get_position(id)
    py = 0.95 + math.sin(t * 3.2) * 0.22

    -- Magnetic pull toward PlayerRover when close
    local player_id = Engine.find_entity("PlayerRover")
    if player_id ~= 0 then
        local rpx, rpy, rpz = Engine.get_position(player_id)
        local dx = rpx - px
        local dz = rpz - pz
        local dist = math.sqrt(dx * dx + dz * dz)
        if dist < 6.0 and dist > 0.1 then
            local pull = (6.0 - dist) * 2.8 * dt
            px = px + (dx / dist) * pull
            pz = pz + (dz / dist) * pull
        end
    end

    Engine.set_position(id, px, py, pz)
end
)LUA");

        // 4. defense_turret.lua
        WriteScriptIfMissing(scriptDir + "/defense_turret.lua", R"LUA(-- Automated Defense Turret AI Script (Hargon Engine Lua 5.4)
local fire_cd = 0.5
local scan_angle = 0.0

function on_init(id)
    fire_cd = 0.4 + (id % 3) * 0.25
end

function on_update(id, dt)
    fire_cd = math.max(0.0, fire_cd - dt)
    scan_angle = scan_angle + 45.0 * dt

    local px, py, pz = Engine.get_position(id)
    local enemy_id, dist = Engine.find_nearest_by_tag(px, py, pz, "Enemy")

    if enemy_id ~= 0 and dist < 24.0 then
        local ex, ey, ez = Engine.get_position(enemy_id)
        local dx = ex - px
        local dz = ez - pz
        local len = math.sqrt(dx * dx + dz * dz)
        if len > 0.01 then
            local nx = dx / len
            local nz = dz / len
            local yaw = math.atan(nx, nz) * (180.0 / 3.14159265)
            Engine.set_rotation(id, 0.0, yaw, 0.0)

            if fire_cd <= 0.0 then
                local hp, max_hp, speed, fire_rate = Engine.get_gameplay_stats(id)
                fire_cd = fire_rate
                Engine.fire_projectile(id, px + nx * 1.15, py + 0.25, pz + nz * 1.15, nx, 0.0, nz, false, 36.0, 15.0)
                Engine.play_sound("turret_shot", 0.55, 1.1)
                Engine.spawn_particles(px + nx * 1.15, py + 0.25, pz + nz * 1.15, 0.2, 1.0, 0.6, 5, 5.0)
            end
        end
    else
        Engine.set_rotation(id, 0.0, scan_angle, 0.0)
    end
end
)LUA");

        // 5. core_reactor.lua
        WriteScriptIfMissing(scriptDir + "/core_reactor.lua", R"LUA(-- Starforge Central Reactor Core Script (Hargon Engine Lua 5.4)
local pulse_t = 0.0

function on_init(id)
    Engine.log("Starforge Core Reactor online.")
end

function on_update(id, dt)
    pulse_t = pulse_t + dt
    local rx, ry, rz = Engine.get_rotation(id)
    Engine.set_rotation(id, rx, ry + 32.0 * dt, rz)

    local glow = 1.8 + 0.7 * math.sin(pulse_t * 3.0)
    Engine.set_emissive(id, 0.15, 0.82, 1.0, glow)
end
)LUA");

        // 6. skyway_runner.lua (Non-combat 3D gravity platformer & time-trial controller)
        WriteScriptIfMissing(scriptDir + "/skyway_runner.lua", R"LUA(-- Skyway 3D Gravity Platformer Script (Hargon Engine Lua 5.4)
local respawn_x, respawn_y, respawn_z = 0.0, 2.0, -18.5
local jump_cooldown = 0.0

function on_init(id)
    Engine.log("Skyway Runner initialized (Gravity Platformer Mode, Entity " .. tostring(id) .. ")")
    Engine.create_timer("course_hint", 1.5)
end

function on_update(id, dt)
    jump_cooldown = math.max(0.0, jump_cooldown - dt)
    local hp, max_hp, speed, _ = Engine.get_gameplay_stats(id)
    local rx, ry, rz = Engine.get_rotation(id)

    -- Steering
    local turn_speed = 140.0
    if Engine.is_key_down("A") or Engine.is_key_down("LEFT") then
        ry = ry - turn_speed * dt
    end
    if Engine.is_key_down("D") or Engine.is_key_down("RIGHT") then
        ry = ry + turn_speed * dt
    end
    Engine.set_rotation(id, rx, ry, rz)

    -- Forward thrust + Turbo Shift boost preserving vertical gravity velocity
    local fx, fy, fz = Engine.get_forward(id)
    local thrust = 0.0
    if Engine.is_key_down("W") or Engine.is_key_down("UP") then
        thrust = 1.0
    elseif Engine.is_key_down("S") or Engine.is_key_down("DOWN") then
        thrust = -0.65
    end
    local boost = Engine.is_key_down("SHIFT") and 1.45 or 1.0

    local vx, vy, vz = Engine.get_velocity(id)
    if thrust ~= 0.0 then
        vx = fx * speed * thrust * boost
        vz = fz * speed * thrust * boost
    end

    -- Platform Jump via Physics is_on_floor + Godot-style Timer SFX rate limit
    local grounded = Engine.is_on_floor(id)
    if Engine.is_key_down("SPACE") and grounded and jump_cooldown <= 0.0 then
        vy = 11.2
        jump_cooldown = 0.25
        local px, py, pz = Engine.get_position(id)
        Engine.spawn_particles(px, py - 0.6, pz, 1.0, 0.78, 0.25, 10, 5.5)
        if Engine.is_timer_ready("jump_sfx") then
            Engine.create_timer("jump_sfx", 0.25)
            Engine.play_sound_3d("wave_start", px, py, pz, 0.75, 1.25, 0, 10.0, 55.0, 0.08)
        end
    end

    Engine.set_velocity(id, vx, vy, vz)

    -- Cloud Sea pit fallback & checkpoint respawn
    local px, py, pz = Engine.get_position(id)
    if py < -6.5 then
        Engine.set_position(id, respawn_x, respawn_y, respawn_z)
        Engine.set_velocity(id, 0.0, 0.0, 0.0)
        Engine.play_sound("hit", 0.7, 0.9)
        Engine.spawn_particles(respawn_x, respawn_y, respawn_z, 1.0, 0.4, 0.2, 14, 7.0)
    elseif grounded and pz > 0.0 then
        -- Advance soft checkpoint across middle/upper platforms
        respawn_x, respawn_y, respawn_z = px, py + 1.2, pz
    end
end
)LUA");
    }
}
