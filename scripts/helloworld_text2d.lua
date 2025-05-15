-- script.lua

function update(time)
    print("Lua update called with time: " .. time)
    -- Render "Hello World" text
    draw_text(100, 100, "Hello World", 1.0, 1.0, 1.0, 1.0)
    print("Drew text 'Hello World' at x=100, y=100")
end

