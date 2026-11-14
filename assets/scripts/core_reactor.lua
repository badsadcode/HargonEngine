-- Starforge Central Reactor Core Script (Hargon Engine Lua 5.4)
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
