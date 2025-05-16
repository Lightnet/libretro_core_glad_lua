-- Global to store texture ID
local texture_id = nil
local texture_width = nil
local texture_height = nil

function update(time)
    print("Lua update called with time: " .. time)

    -- if not texture_id then
    --     texture_id, texture_width, texture_height = load_image("image.png")
    --     if texture_id then
    --         print("Loaded image: texture_id=" .. texture_id .. ", size=" .. texture_width .. "x" .. texture_height)
    --     else
    --         print("Failed to load image")
    --     end
    -- end

    local vertices = {
        {-150, -150}, -- Bottom-left
        { 150, -150}, -- Bottom-right
        {-150,  150}, -- Top-left
        { 150,  150}  -- Top-right
    }
    local x, y = -100, -100 -- Center of 512x512 viewport
    local rotation = time * 30
    local r, g, b = 0.0, 0.5, 0.0
    if get_input(RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) then
        g, b = 0.0, 1.0
    end
    if get_input(RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) then
        r, g = 1.0, 0.0
    end
    draw_custom_quad(vertices, x, y, rotation, r, g, b, 1.0)

    -- if texture_id then
    --     draw_texture(texture_id, 256, 256, texture_width, texture_height, rotation, 1.0, 1.0, 1.0, 1.0)
    -- end

    draw_text(100, 100, "Hello Worldff", 1.0, 1.0, 1.0, 1.0)
end