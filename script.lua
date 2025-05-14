-- script.lua
local RETRO_DEVICE_JOYPAD = 1
local RETRO_DEVICE_ID_JOYPAD_A = 8
local RETRO_DEVICE_ID_JOYPAD_B = 0

function update(time)
    print("Lua update called with time: " .. time)

    local scale = 0.8 + 0.2 * math.sin(time * 2.0)
    local width = 512 * scale
    local height = 512 * scale
    local x = (512 - width) * 0.5
    local y = (512 - height) * 0.5

    local r, g, b = 0.0, 0.5, 0.0 -- Default green
    local a_pressed = get_input(RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A)
    local b_pressed = get_input(RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B)
    -- print(string.format("Lua input: A(id=%d)=%s, B(id=%d)=%s",
    --                    RETRO_DEVICE_ID_JOYPAD_A, tostring(a_pressed),
    --                    RETRO_DEVICE_ID_JOYPAD_B, tostring(b_pressed)))

    if a_pressed then
        print("Setting blue for A")
        g, b = 0.0, 1.0
    end
    if b_pressed then
        print("Setting red for B")
        r, g = 1.0, 0.0
    end

    draw_quad(x, y, width, height, r, g, b, 1.0)
    -- print("Drew quad at x=" .. x .. ", y=" .. y .. ", w=" .. width .. ", h=" .. height)
end