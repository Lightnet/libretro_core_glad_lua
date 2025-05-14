#include "module_lua.h"
#include "module_opengl.h" // For draw_solid_quad
#include <stdio.h>

// Global variables
static lua_State *L = NULL;
static retro_input_state_t input_state_cb;

// External logging function
extern void core_log(enum retro_log_level level, const char *fmt, ...);

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
   module_opengl_draw_solid_quad(x, y, w, h, r, g, b, a, 512, 512); // HW_WIDTH, HW_HEIGHT
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
      core_log(RETRO_LOG_DEBUG, "lua_get_input: device=%d, index=%d, id=%d, state=%d",
               device, index, id, state);
   } else {
      core_log(RETRO_LOG_WARN, "No input_state_cb set for device=%d, index=%d, id=%d",
               device, index, id);
   }
   lua_pushboolean(L, state != 0);
   return 1;
}

bool module_lua_init(retro_input_state_t input_cb) {
   if (L) {
      core_log(RETRO_LOG_INFO, "Lua already initialized, skipping");
      return true;
   }

   input_state_cb = input_cb;

   L = luaL_newstate();
   if (!L) {
      core_log(RETRO_LOG_ERROR, "Failed to create Lua state");
      return false;
   }

   luaL_openlibs(L); // Load standard Lua libraries

   // Register C functions
   lua_register(L, "draw_quad", lua_draw_quad);
   lua_register(L, "get_input", lua_get_input);

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
      lua_pop(L, 1);
      core_log(RETRO_LOG_WARN, "No Lua update function found");
   }
}

lua_State *module_lua_get_state(void) {
   return L;
}