// module_lua.c
#include "module_lua.h"
#include "module_opengl.h"
#include <stdio.h>
#include <stdlib.h>

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


// Lua-exposed function: load_image(asset_name)
static int lua_load_image(lua_State *L) {
   const char *asset_name = luaL_checkstring(L, 1);
   int width, height;
   GLuint texture_id = module_opengl_load_image(asset_name, &width, &height);
   if (texture_id == 0) {
      lua_pushnil(L);
      core_log(RETRO_LOG_ERROR, "Lua: Failed to load image %s", asset_name);
   } else {
      lua_pushinteger(L, texture_id);
      lua_pushinteger(L, width);
      lua_pushinteger(L, height);
      core_log(RETRO_LOG_INFO, "Lua: Loaded image %s as texture %u (%dx%d)", asset_name, texture_id, width, height);
   }
   return 3; // Return texture_id, width, height
}


// Lua-exposed function: draw_texture(texture_id, x, y, w, h, rotation, r, g, b, a)
static int lua_draw_texture(lua_State *L) {
   GLuint texture_id = (GLuint)luaL_checkinteger(L, 1);
   float x = (float)luaL_checknumber(L, 2);
   float y = (float)luaL_checknumber(L, 3);
   float w = (float)luaL_checknumber(L, 4);
   float h = (float)luaL_checknumber(L, 5);
   float rotation = (float)luaL_checknumber(L, 6);
   float r = (float)luaL_checknumber(L, 7);
   float g = (float)luaL_checknumber(L, 8);
   float b = (float)luaL_checknumber(L, 9);
   float a = (float)luaL_checknumber(L, 10);
   module_opengl_draw_texture(texture_id, x, y, w, h, rotation, r, g, b, a, 512, 512);
   return 0;
}


// Lua-exposed function: draw_quad(x, y, w, h, rotation, r, g, b, a)
static int lua_draw_quad(lua_State *L) {
   float x = (float)luaL_checknumber(L, 1);
   float y = (float)luaL_checknumber(L, 2);
   float w = (float)luaL_checknumber(L, 3);
   float h = (float)luaL_checknumber(L, 4);
   float rotation = (float)luaL_checknumber(L, 5); // New: rotation in degrees
   float r = (float)luaL_checknumber(L, 6);
   float g = (float)luaL_checknumber(L, 7);
   float b = (float)luaL_checknumber(L, 8);
   float a = (float)luaL_checknumber(L, 9);
   module_opengl_draw_solid_quad(x, y, w, h, rotation, r, g, b, a, 512, 512);
   return 0;
}





static int lua_draw_custom_quad(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float rotation = (float)luaL_checknumber(L, 4);
    float r = (float)luaL_checknumber(L, 5);
    float g = (float)luaL_checknumber(L, 6);
    float b = (float)luaL_checknumber(L, 7);
    float a = (float)luaL_checknumber(L, 8);

    lua_Unsigned num_vertices = lua_rawlen(L, 1);
    if (num_vertices < 3) {
        core_log(RETRO_LOG_ERROR, "At least 3 vertices required for custom quad");
        return 0;
    }
    if (num_vertices > INT_MAX) {
        core_log(RETRO_LOG_ERROR, "Number of vertices (%llu) exceeds maximum allowed (%d)", num_vertices, INT_MAX);
        return 0;
    }

    float *vertices = (float *)malloc(num_vertices * 2 * sizeof(float));
    if (!vertices) {
        core_log(RETRO_LOG_ERROR, "Failed to allocate memory for vertices");
        return 0;
    }

    for (lua_Unsigned i = 0; i < num_vertices; i++) {
        lua_rawgeti(L, 1, i + 1);
        luaL_checktype(L, -1, LUA_TTABLE);
        lua_rawgeti(L, -1, 1);
        lua_rawgeti(L, -2, 2);
        vertices[i * 2] = (float)luaL_checknumber(L, -2);
        vertices[i * 2 + 1] = (float)luaL_checknumber(L, -1);
        lua_pop(L, 3);
    }

    module_opengl_draw_custom_quad(vertices, (int)num_vertices, x, y, rotation, r, g, b, a, 512, 512);

    free(vertices);
    return 0;
}





// Lua-exposed function: draw_text(x, y, text, r, g, b, a)
static int lua_draw_text(lua_State *L) {
   float x = (float)luaL_checknumber(L, 1);
   float y = (float)luaL_checknumber(L, 2);
   const char *text = luaL_checkstring(L, 3);
   float r = (float)luaL_checknumber(L, 4);
   float g = (float)luaL_checknumber(L, 5);
   float b = (float)luaL_checknumber(L, 6);
   float a = (float)luaL_checknumber(L, 7);
   module_opengl_draw_text(x, y, text, r, g, b, a, 512, 512);
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

// Register Libretro input constants as Lua globals
static void register_libretro_constants(lua_State *L) {
    // Device types
    lua_pushinteger(L, RETRO_DEVICE_JOYPAD);
    lua_setglobal(L, "RETRO_DEVICE_JOYPAD");

    // Joypad button IDs
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_B);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_B");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_Y);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_Y");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_SELECT);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_SELECT");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_START);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_START");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_UP);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_UP");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_DOWN);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_DOWN");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_LEFT);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_LEFT");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_RIGHT);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_RIGHT");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_A);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_A");
    lua_pushinteger(L,RETRO_DEVICE_ID_JOYPAD_X);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_X");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_L);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_L");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_R);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_R");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_L2);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_L2");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_R2);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_R2");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_L3);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_L3");
    lua_pushinteger(L, RETRO_DEVICE_ID_JOYPAD_R3);
    lua_setglobal(L, "RETRO_DEVICE_ID_JOYPAD_R3");

    core_log(RETRO_LOG_INFO, "Registered Libretro input constants in Lua");
}


// Lua-exposed function: free_texture(texture_id)
static int lua_free_texture(lua_State *L) {
   GLuint texture_id = (GLuint)luaL_checkinteger(L, 1);
   module_opengl_free_texture(texture_id);
   core_log(RETRO_LOG_INFO, "Lua: Freed texture %u", texture_id);
   return 0;
}



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
   lua_register(L, "draw_text", lua_draw_text);
   lua_register(L, "draw_custom_quad", lua_draw_custom_quad);
   lua_register(L, "load_image", lua_load_image);
   lua_register(L, "draw_texture", lua_draw_texture);
   lua_register(L, "free_texture", lua_free_texture);

   // Register Libretro constants
   register_libretro_constants(L);

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

   lua_getglobal(L, "_G");
   lua_pushcfunction(L, lua_core_print);
   lua_setfield(L, -2, "print");
   lua_pop(L, 1);

   lua_register(L, "draw_quad", lua_draw_quad);
   lua_register(L, "get_input", lua_get_input);
   lua_register(L, "draw_text", lua_draw_text);
   lua_register(L, "draw_custom_quad", lua_draw_custom_quad);
   lua_register(L, "load_image", lua_load_image);
   lua_register(L, "draw_texture", lua_draw_texture);
   lua_register(L, "free_texture", lua_free_texture);

   register_libretro_constants(L);

   if (luaL_loadbuffer(L, script_data, script_size, "script.lua") != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
      const char *err = lua_tostring(L, -1);
      core_log(RETRO_LOG_ERROR, "Failed to load Lua script from buffer: %s", err);
      lua_pop(L, 1);
      lua_close(L);
      L = NULL;
      return false;
   }

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