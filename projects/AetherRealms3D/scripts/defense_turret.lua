-- Automated Defense Turret AI Script (Hargon Engine Lua 5.4)
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
