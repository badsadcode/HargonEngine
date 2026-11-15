-- Energy Crystal Rotation & Magnetic Pull Script (Hargon Engine Lua 5.4)
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
