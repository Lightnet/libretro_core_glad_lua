-- script.lua

function update(time)
    print("Lua update called with time: " .. time)

    local vertices = {
        {-150, -150}, -- Bottom-left
        { 150, -150}, -- Bottom-right
        {-150,  150}, -- Top-left
        { 150,  150}  -- Top-right
    }

    local x, y = 0, 0
    local rotation = 45
    local r, g, b = 0.0, 0.5, 0.0
    if get_input(RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) then
        g, b = 0.0, 1.0
    end
    if get_input(RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) then
        r, g = 1.0, 0.0
    end

    draw_custom_quad(vertices, x, y, rotation, r, g, b, 1.0)
    print("Drew custom quad at x=" .. x .. ", y=" .. y)
end
