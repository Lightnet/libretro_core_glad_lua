local textures = {}

function load_texture(asset_name)
    local id, w, h = load_image(asset_name)
    if id then
        textures[asset_name] = {id = id, width = w, height = h}
        print("Loaded " .. asset_name .. ": id=" .. id .. ", size=" .. w .. "x" .. h)
    else
        print("Failed to load " .. asset_name)
    end
end

function update(time)
    print("Lua update called with time: " .. time)
    if not textures["image.png"] then
        load_texture("image.png")
    end

    local vertices = {
        {-150, -150}, {150, -150}, {-150, 150}, {150, 150}
    }
    local x, y = 0, 0
    local rotation = time * 30
    local r, g, b = 0.0, 0.5, 0.0
    if get_input(RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) then
        g, b = 0.0, 1.0
    end
    if get_input(RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) then
        r, g = 1.0, 0.0
    end
    draw_custom_quad(vertices, x, y, rotation, r, g, b, 1.0)

    if textures["image.png"] then
        local tex = textures["image.png"]
        draw_texture(tex.id, 0, 0, tex.width, tex.height, rotation, 1.0, 1.0, 1.0, 1.0)
    else
        draw_quad(0, 0, 64, 64, 0, 1.0, 0.0, 0.0, 1.0) -- Fallback
    end
    
    draw_text(100, 100, "Hello World", 1.0, 1.0, 1.0, 1.0)
end

function cleanup()
    for name, tex in pairs(textures) do
        free_texture(tex.id)
        print("Freed texture " .. name)
    end
    textures = {}
end