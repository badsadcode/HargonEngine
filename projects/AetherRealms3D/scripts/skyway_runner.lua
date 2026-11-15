-- Skyway 3D Gravity Platformer Script (Hargon Engine Lua 5.4)
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
