-- Enemy Seeker Drone AI Script (Hargon Engine Lua 5.4)
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
