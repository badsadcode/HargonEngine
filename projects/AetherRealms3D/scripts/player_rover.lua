-- Player Hover-Rover Controller Script (Hargon Engine Lua 5.4)
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
