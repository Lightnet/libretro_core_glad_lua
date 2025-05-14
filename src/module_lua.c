#include "module_lua.h"
#include "module_opengl.h"
#include <stdio.h>

// Global variables
static lua_State *L = NULL;

// External input callback from main.c
extern retro_input_state_t input_state_cb;

// External logging function
extern void core_log(enum retro_log_level level, const char *fmt, ...);

// Custom Lua print function to redirect to core_log
static int lua_core_print(lua_State *L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        const char *str = lua_tostring(L, i);
        if (str) {
            core_log(RETRO_LOG_INFO, "Lua: %s\n", str);
        }
    }
    return 0;
}

// Lua-exposed function: draw_quad(x, y, w, h, r, g, b, a)
static int lua_draw_quad(lua_State *L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    float r = (float)luaL_checknumber(L, 5);
    float g = (float)luaL_checknumber(L, 6);
    float b = (float)luaL_checknumber(L, 7);
    float a = (float)luaL_checknumber(L, 8);
    module_opengl_draw_solid_quad(x, y, w, h, r, g, b, a, 512, 512);
    return 0;
}

// Lua-exposed function: get_input(device, index, id)
static int lua_get_input(lua_State *L) {
    int device = (int)luaL_checkinteger(L, 1);
    int index = (int)luaL_checkinteger(L, 2);
    int id = (int)luaL_checkinteger(L, 3);
    int state = 0;
    if (input_state_cb) {
        state = input_state_cb(index, device, 0, id);
        core_log(RETRO_LOG_DEBUG, "lua_get_input: device=%d, index=%d, id=%d, state=%d\n",
                 device, index, id, state);
    } else {
        core_log(RETRO_LOG_WARN, "No input_state_cb set for device=%d, index=%d, id=%d\n",
                 device, index, id);
    }
    lua_pushboolean(L, state != 0);
    return 1;
}

// static int lua_load_asset(lua_State *L) {
//     const char *asset_name = luaL_checkstring(L, 1);
//     char *asset_data = NULL;
//     size_t asset_size = 0;
//     if (extract_asset_from_zip(asset_name, &asset_data, &asset_size)) {
//         // Return asset data as a Lua string (or process further, e.g., load as texture)
//         lua_pushlstring(L, asset_data, asset_size);
//         free(asset_data);
//         return 1;
//     }
//     lua_pushnil(L);
//     return 1;
// }


bool module_lua_init(void) {
    if (L) {
        core_log(RETRO_LOG_INFO, "Lua already initialized, skipping");
        return true;
    }

    core_log(RETRO_LOG_INFO, "Lua init with input_state_cb: %p", input_state_cb);

    L = luaL_newstate();
    if (!L) {
        core_log(RETRO_LOG_ERROR, "Failed to create Lua state");
        return false;
    }

    luaL_openlibs(L);

    // Override print function
    lua_getglobal(L, "_G");
    lua_pushcfunction(L, lua_core_print);
    lua_setfield(L, -2, "print");
    lua_pop(L, 1);

    // Register C functions
    lua_register(L, "draw_quad", lua_draw_quad);
    lua_register(L, "get_input", lua_get_input);
    //lua_register(L, "load_asset", lua_load_asset);

    // Load Lua script
    const char *script_path = "script.lua";
    if (luaL_dofile(L, script_path) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        core_log(RETRO_LOG_ERROR, "Failed to load Lua script '%s': %s", script_path, err);
        lua_pop(L, 1);
        lua_close(L);
        L = NULL;
        return false;
    }

    // Verify update function exists
    lua_getglobal(L, "update");
    if (!lua_isfunction(L, -1)) {
        core_log(RETRO_LOG_ERROR, "No 'update' function found in script.lua");
        lua_pop(L, 1);
        lua_close(L);
        L = NULL;
        return false;
    }
    lua_pop(L, 1);

    core_log(RETRO_LOG_INFO, "Lua initialized and script loaded");
    return true;
}

void module_lua_deinit(void) {
    if (L) {
        lua_close(L);
        L = NULL;
        core_log(RETRO_LOG_INFO, "Lua deinitialized");
    }
}

void module_lua_update(float animation_time) {
    if (!L) {
        core_log(RETRO_LOG_ERROR, "Lua state not initialized");
        return;
    }

    lua_getglobal(L, "update");
    if (lua_isfunction(L, -1)) {
        lua_pushnumber(L, animation_time);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            core_log(RETRO_LOG_ERROR, "Lua update error: %s", err);
            lua_pop(L, 1);
        }
    } else {
        core_log(RETRO_LOG_WARN, "No Lua update function found");
        lua_pop(L, 1);
    }
}

lua_State *module_lua_get_state(void) {
    return L;
}

bool module_lua_init_from_buffer(const char *script_data, size_t script_size) {
    if (L) {
        core_log(RETRO_LOG_INFO, "Lua already initialized, skipping");
        return true;
    }

    core_log(RETRO_LOG_INFO, "Lua init from buffer with input_state_cb: %p", input_state_cb);

    L = luaL_newstate();
    if (!L) {
        core_log(RETRO_LOG_ERROR, "Failed to create Lua state");
        return false;
    }

    luaL_openlibs(L);

    // Override print function
    lua_getglobal(L, "_G");
    lua_pushcfunction(L, lua_core_print);
    lua_setfield(L, -2, "print");
    lua_pop(L, 1);

    // Register C functions
    lua_register(L, "draw_quad", lua_draw_quad);
    lua_register(L, "get_input", lua_get_input);

    // Load Lua script from buffer
    if (luaL_loadbuffer(L, script_data, script_size, "script.lua") != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        core_log(RETRO_LOG_ERROR, "Failed to load Lua script from buffer: %s", err);
        lua_pop(L, 1);
        lua_close(L);
        L = NULL;
        return false;
    }

    // Verify update function exists
    lua_getglobal(L, "update");
    if (!lua_isfunction(L, -1)) {
        core_log(RETRO_LOG_ERROR, "No 'update' function found in script");
        lua_pop(L, 1);
        lua_close(L);
        L = NULL;
        return false;
    }
    lua_pop(L, 1);

    core_log(RETRO_LOG_INFO, "Lua initialized and script loaded from buffer");
    return true;
}
