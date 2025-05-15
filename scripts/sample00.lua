-- script.lua
-- local RETRO_DEVICE_JOYPAD = 1 --remove is add in global variable c to lua api 
-- local RETRO_DEVICE_ID_JOYPAD_A = 8
-- local RETRO_DEVICE_ID_JOYPAD_B = 0

local rotation = 0

function update(time)
    print("Lua update called with time: " .. time)

    -- Quad rendering
    local max_size = 300 -- Max size to fit when rotated (diagonal < 512)
    local scale = 0.8 + 0.2 * math.sin(time * 2.0)
    local width = max_size * scale
    local height = max_size * scale
    local x = 100
    local y = 100
    rotation = 45 -- Fixed 45-degree rotation for diamond shape
    -- rotation = rotation + 1 -- rotation quad shape

    local r, g, b = 0.0, 0.5, 0.0 -- Default green

    if get_input(RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) then
        print("Setting blue for A")
        g, b = 0.0, 1.0
    end
    if get_input(RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) then
        print("Setting red for B")
        r, g = 1.0, 0.0
    end

    draw_quad(x, y, width, height, rotation, r, g, b, 1.0)
    print("Drew quad at x=" .. x .. ", y=" .. y .. ", w=" .. width .. ", h=" .. height .. ", rotation=" .. rotation)

    -- Render "Hello World" text
    draw_text(100, 100, "Hello World", 1.0, 1.0, 1.0, 1.0)
    print("Drew text 'Hello World' at x=100, y=100")
end

