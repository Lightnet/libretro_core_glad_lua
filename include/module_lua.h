// module_lua.h
#ifndef MODULE_LUA_H
#define MODULE_LUA_H

#include <libretro.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// Initialize Lua and load script from file
bool module_lua_init(void);

// Initialize Lua and load script from memory buffer
bool module_lua_init_from_buffer(const char *script_data, size_t script_size);

// Deinitialize Lua
void module_lua_deinit(void);

// Run Lua update function
void module_lua_update(float animation_time);

// Get Lua state for external use
lua_State *module_lua_get_state(void);

#endif // MODULE_LUA_H