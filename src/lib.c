#include <libretro.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "module_opengl.h"
#include <miniz.h>
#include "module_lua.h"

// Framebuffer dimensions
#define WIDTH 320
#define HEIGHT 240
#define HW_WIDTH 512
#define HW_HEIGHT 512

// Global variables
static retro_environment_t environ_cb;
static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
retro_input_state_t input_state_cb;
static struct retro_hw_render_callback hw_render;
static bool initialized = false;
static FILE *log_file = NULL;
static float animation_time = 0.0f;
static bool use_default_fbo = false;
static char zip_file_path[512] = {0}; // Store zip file path

// File-based logging
static void fallback_log(const char *level, const char *msg) {
   if (!log_file) {
      log_file = fopen("core.log", "a");
      if (!log_file) {
         fprintf(stderr, "[ERROR] Failed to open core.log\n");
         return;
      }
   }
   fprintf(log_file, "[%s] %s\n", level, msg);
   fflush(log_file);
   fprintf(stderr, "[%s] %s\n", level, msg);
}

static void fallback_log_format(const char *level, const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);
   if (!log_file) {
      log_file = fopen("core.log", "a");
      if (!log_file) {
         fprintf(stderr, "[ERROR] Failed to open core.log\n");
         va_end(args);
         return;
      }
   }
   fprintf(log_file, "[%s] ", level);
   vfprintf(log_file, fmt, args);
   fprintf(log_file, "\n");
   fflush(log_file);
   fprintf(stderr, "[%s] ", level);
   vfprintf(stderr, fmt, args);
   fprintf(stderr, "\n");
   va_end(args);
}

// Unified logging helper
void core_log(enum retro_log_level level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    const char *level_str;
    switch (level) {
        case RETRO_LOG_DEBUG: level_str = "DEBUG"; break;
        case RETRO_LOG_INFO:  level_str = "INFO";  break;
        case RETRO_LOG_WARN:  level_str = "WARN";  break;
        case RETRO_LOG_ERROR: level_str = "ERROR"; break;
        default: level_str = "UNKNOWN"; break;
    }

    if (log_cb) {
        char msg[512];
        vsnprintf(msg, sizeof(msg), fmt, args);
        log_cb(level, "[%s] %s", level_str, msg);
    } else {
        fallback_log_format(level_str, fmt, args);
    }

    va_end(args);
}


// Extract script.lua from zip and return its contents
static bool extract_lua_script(const void *zip_data, size_t zip_size, char **script_data, size_t *script_size) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    // Initialize zip reader
    if (!mz_zip_reader_init_mem(&zip_archive, zip_data, zip_size, 0)) {
        core_log(RETRO_LOG_ERROR, "Failed to initialize zip reader");
        return false;
    }

    // Find script.lua in the zip
    int file_index = mz_zip_reader_locate_file(&zip_archive, "script.lua", NULL, 0);
    if (file_index < 0) {
        core_log(RETRO_LOG_ERROR, "script.lua not found in zip");
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    // Get file stats
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip_archive, file_index, &file_stat)) {
        core_log(RETRO_LOG_ERROR, "Failed to get script.lua stats");
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    // Allocate memory for script
    *script_data = (char *)malloc(file_stat.m_uncomp_size + 1);
    if (!*script_data) {
        core_log(RETRO_LOG_ERROR, "Failed to allocate memory for script.lua");
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    // Extract script.lua
    if (!mz_zip_reader_extract_to_mem(&zip_archive, file_index, *script_data, file_stat.m_uncomp_size, 0)) {
        core_log(RETRO_LOG_ERROR, "Failed to extract script.lua");
        free(*script_data);
        *script_data = NULL;
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    (*script_data)[file_stat.m_uncomp_size] = '\0'; // Null-terminate
    *script_size = file_stat.m_uncomp_size;

    core_log(RETRO_LOG_INFO, "Successfully extracted script.lua (%zu bytes)", *script_size);
    mz_zip_reader_end(&zip_archive);
    return true;
}

// Extract script.lua from zip file at path
static bool extract_lua_script_from_file(const char *zip_path, char **script_data, size_t *script_size) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    // Initialize zip reader from file
    if (!mz_zip_reader_init_file(&zip_archive, zip_path, 0)) {
        core_log(RETRO_LOG_ERROR, "Failed to initialize zip reader for file: %s", zip_path);
        return false;
    }

    // Find script.lua in the zip
    int file_index = mz_zip_reader_locate_file(&zip_archive, "script.lua", NULL, 0);
    if (file_index < 0) {
        core_log(RETRO_LOG_ERROR, "script.lua not found in zip: %s", zip_path);
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    // Get file stats
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip_archive, file_index, &file_stat)) {
        core_log(RETRO_LOG_ERROR, "Failed to get script.lua stats from zip: %s", zip_path);
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    // Allocate memory for script
    *script_data = (char *)malloc(file_stat.m_uncomp_size + 1);
    if (!*script_data) {
        core_log(RETRO_LOG_ERROR, "Failed to allocate memory for script.lua");
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    // Extract script.lua
    if (!mz_zip_reader_extract_to_mem(&zip_archive, file_index, *script_data, file_stat.m_uncomp_size, 0)) {
        core_log(RETRO_LOG_ERROR, "Failed to extract script.lua from zip: %s", zip_path);
        free(*script_data);
        *script_data = NULL;
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    (*script_data)[file_stat.m_uncomp_size] = '\0'; // Null-terminate
    *script_size = file_stat.m_uncomp_size;

    core_log(RETRO_LOG_INFO, "Successfully extracted script.lua (%zu bytes) from %s", *script_size, zip_path);
    mz_zip_reader_end(&zip_archive);
    return true;
}

bool extract_asset_from_zip(const char *asset_name, char **asset_data, size_t *asset_size) {
    if (!zip_file_path[0]) {
        core_log(RETRO_LOG_ERROR, "No zip file path set for asset extraction");
        return false;
    }

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_file(&zip_archive, zip_file_path, 0)) {
        core_log(RETRO_LOG_ERROR, "Failed to initialize zip reader for asset: %s", zip_file_path);
        return false;
    }

    int file_index = mz_zip_reader_locate_file(&zip_archive, asset_name, NULL, 0);
    if (file_index < 0) {
        core_log(RETRO_LOG_ERROR, "Asset %s not found in zip: %s", asset_name, zip_file_path);
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip_archive, file_index, &file_stat)) {
        core_log(RETRO_LOG_ERROR, "Failed to get asset %s stats", asset_name);
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    *asset_data = (char *)malloc(file_stat.m_uncomp_size);
    if (!*asset_data) {
        core_log(RETRO_LOG_ERROR, "Failed to allocate memory for asset %s", asset_name);
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    if (!mz_zip_reader_extract_to_mem(&zip_archive, file_index, *asset_data, file_stat.m_uncomp_size, 0)) {
        core_log(RETRO_LOG_ERROR, "Failed to extract asset %s", asset_name);
        free(*asset_data);
        *asset_data = NULL;
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    *asset_size = file_stat.m_uncomp_size;
    core_log(RETRO_LOG_INFO, "Successfully extracted asset %s (%zu bytes)", asset_name, *asset_size);
    mz_zip_reader_end(&zip_archive);
    return true;
}

// Set environment
void retro_set_environment(retro_environment_t cb) {
   environ_cb = cb;
   if (!cb) {
      core_log(RETRO_LOG_ERROR, "retro_set_environment: Null environment callback");
      return;
   }

  // disable start core
  bool contentless = false;
  environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &contentless);

  //  bool contentless = true;
  //  if (environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &contentless)) {
  //     core_log(RETRO_LOG_INFO, "Content-less support enabled");
  //  } else {
  //     core_log(RETRO_LOG_ERROR, "Failed to set content-less support");
  //  }
}

// Video refresh callback
void retro_set_video_refresh(retro_video_refresh_t cb) {
   video_cb = cb;
   core_log(RETRO_LOG_INFO, "Video refresh callback set");
}

// Input callbacks
void retro_set_input_poll(retro_input_poll_t cb) {
   input_poll_cb = cb;
   printf("Input poll callback set: %p\n", cb);
}

void retro_set_input_state(retro_input_state_t cb) {
    input_state_cb = cb;
    core_log(RETRO_LOG_INFO, "Input state callback set: %p", cb);
    // Reinitialize Lua if it was initialized before input callback was set
    if (module_lua_get_state() && cb) {
        module_lua_deinit();
        if (!module_lua_init()) {
            core_log(RETRO_LOG_WARN, "Failed to reinitialize Lua after input callback set");
        } else {
            core_log(RETRO_LOG_INFO, "Lua reinitialized with input_state_cb: %p", cb);
        }
    }
}


// Stubbed audio callbacks
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }

// Initialize core
void retro_init(void) {
   initialized = true;
   struct retro_log_callback logging;
   if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging)) {
      log_cb = logging.log;
   }
   core_log(RETRO_LOG_INFO, "Hello World core initialized");
   printf("Input constants: RETRO_DEVICE_JOYPAD=%d, A=%d, B=%d\n",
            RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B);
}

// Deinitialize core
void retro_deinit(void) {
   module_opengl_deinit();
   module_lua_deinit();
   if (log_file) {
      fclose(log_file);
      log_file = NULL;
   }
   initialized = false;
   core_log(RETRO_LOG_INFO, "Core deinitialized");
}

// System info
void retro_get_system_info(struct retro_system_info *info) {
   memset(info, 0, sizeof(*info));
   info->library_name = "Libretro Core Glad Lua";
   info->library_version = "1.0";
   info->need_fullpath = true;
   info->block_extract = true;
   info->valid_extensions = "zip";
   core_log(RETRO_LOG_INFO, "System info: %s v%s", info->library_name, info->library_version);
}

// AV info
void retro_get_system_av_info(struct retro_system_av_info *info) {
   memset(info, 0, sizeof(*info));
   info->geometry.base_width = WIDTH;
   info->geometry.base_height = HEIGHT;
   info->geometry.max_width = HW_WIDTH;
   info->geometry.max_height = HW_HEIGHT;
   info->geometry.aspect_ratio = (float)WIDTH / HEIGHT;
   info->timing.fps = 60.0;
   info->timing.sample_rate = 48000.0;
   core_log(RETRO_LOG_INFO, "AV info: %dx%d, max %dx%d, %.2f fps",
            WIDTH, HEIGHT, HW_WIDTH, HW_HEIGHT, info->timing.fps);
}

// Controller port
void retro_set_controller_port_device(unsigned port, unsigned device) {
   core_log(RETRO_LOG_INFO, "Controller port device set: port=%u, device=%u", port, device);
}

// Reset core
void retro_reset(void) {
   core_log(RETRO_LOG_INFO, "Core reset");
}

// Load game
bool retro_load_game(const struct retro_game_info *game) {
    if (!environ_cb) {
        core_log(RETRO_LOG_ERROR, "Environment callback not set");
        return false;
    }

    // Set up OpenGL context
    hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
    hw_render.version_major = 3;
    hw_render.version_minor = 3;
    hw_render.context_reset = module_opengl_init;
    hw_render.context_destroy = module_opengl_deinit;
    hw_render.bottom_left_origin = true;
    hw_render.depth = true;
    hw_render.stencil = false;
    hw_render.cache_context = false;
    hw_render.debug_context = true;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        core_log(RETRO_LOG_ERROR, "Failed to set OpenGL context");
        return false;
    }

    module_opengl_set_callbacks(hw_render.get_proc_address, hw_render.get_current_framebuffer, &use_default_fbo);

    // Handle game data (script.zip)
    if (game && game->path) {
        // Store zip file path
        strncpy(zip_file_path, game->path, sizeof(zip_file_path) - 1);
        zip_file_path[sizeof(zip_file_path) - 1] = '\0';
        core_log(RETRO_LOG_INFO, "Zip file path: %s", zip_file_path);

        // Extract and load script.lua
        char *script_data = NULL;
        size_t script_size = 0;
        if (extract_lua_script_from_file(game->path, &script_data, &script_size)) {
            if (!module_lua_init_from_buffer(script_data, script_size)) {
                core_log(RETRO_LOG_WARN, "Failed to initialize Lua with script from zip");
            } else {
                core_log(RETRO_LOG_INFO, "Lua initialized with script from zip");
            }
            free(script_data);
        } else {
            core_log(RETRO_LOG_WARN, "Failed to extract script.lua, falling back to default");
            if (!module_lua_init()) {
                core_log(RETRO_LOG_WARN, "Failed to initialize Lua with default script");
            }
        }
    } else {
        core_log(RETRO_LOG_INFO, "No game path provided, using default script");
        zip_file_path[0] = '\0'; // Clear zip path
        if (!module_lua_init()) {
            core_log(RETRO_LOG_WARN, "Failed to initialize Lua with default script");
        }
    }

    core_log(RETRO_LOG_INFO, "Game loaded");
    return true;
}

// Run frame
void retro_run(void) {
  //  printf("render\n");
   if (!initialized) {
      core_log(RETRO_LOG_ERROR, "Core not initialized");
      return;
   }

   if (!module_opengl_is_initialized()) {
      core_log(RETRO_LOG_ERROR, "OpenGL not initialized, skipping frame");
      return;
   }
  //  printf("gl_initialized\n");

   // Poll input
   if (input_poll_cb) {
      input_poll_cb();
      // core_log(RETRO_LOG_DEBUG, "Input polled");
   } else {
      // core_log(RETRO_LOG_WARN, "No input_poll_cb set");
   }

   // Log input state for A and B
   if (input_state_cb) {
      int a_state = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
      int b_state = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
      core_log(RETRO_LOG_DEBUG, "Direct input check: A(id=%d)=%d, B(id=%d)=%d\n",
               RETRO_DEVICE_ID_JOYPAD_A, a_state, RETRO_DEVICE_ID_JOYPAD_B, b_state);
      for (int i = 0; i < 16; i++) {
         int state = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i);
         if (state) {
            core_log(RETRO_LOG_DEBUG, "Joypad button id=%d pressed\n", i);
         }
      }
   }

   // Bind framebuffer
   module_opengl_bind_framebuffer();
  //  printf("get_current_framebuffer\n");
   module_opengl_check_error("framebuffer binding");

   // Set viewport
   module_opengl_set_viewport();

   // Clear framebuffer
   module_opengl_clear();

   // Increment animation time
   animation_time += 0.016f; // ~60 FPS (1/60 = 0.01667)

   // Run Lua update
    lua_State *L = module_lua_get_state();
    if (L) {
        module_lua_update(animation_time);
    } else {
        // Fallback quad drawing
        float r = 0.0f, g = 0.5f, b = 0.0f; // Default green
        if (input_state_cb) {
            int a_state = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
            int b_state = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
            core_log(RETRO_LOG_DEBUG, "Fallback input state: A(id=%d)=%d, B(id=%d)=%d",
                     RETRO_DEVICE_ID_JOYPAD_A, a_state, RETRO_DEVICE_ID_JOYPAD_B, b_state);
            if (a_state)
                g = 0.0f, b = 1.0f; // Blue
            if (b_state)
                r = 1.0f, g = 0.0f; // Red
        }

        float scale = 0.8f + 0.2f * sinf(animation_time * 2.0f);
        float quad_width = HW_WIDTH * scale;
        float quad_height = HW_HEIGHT * scale;
        float quad_x = (HW_WIDTH - quad_width) * 0.5f;
        float quad_y = (HW_HEIGHT - quad_height) * 0.5f;

        module_opengl_draw_solid_quad(quad_x, quad_y, quad_width, quad_height, r, g, b, 1.0f, HW_WIDTH, HW_HEIGHT);
        module_opengl_check_error("draw_solid_quad");
    }

   // Log FBO binding
   GLint current_fbo;
   glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
  //  core_log(RETRO_LOG_DEBUG, "Current FBO binding after rendering: %d", current_fbo);

   // Unbind framebuffer
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   module_opengl_check_error("unbind framebuffer");

   // Present frame
   if (video_cb) {
      video_cb(RETRO_HW_FRAME_BUFFER_VALID, 960, 720, 0);
      // core_log(RETRO_LOG_DEBUG, "Frame presented with size 960x720");
   } else {
      // core_log(RETRO_LOG_ERROR, "No video callback set");
   }
}

// Load special game
bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) {
    core_log(RETRO_LOG_INFO, "retro_load_game_special called with game_type=%u, num_info=%zu", game_type, num_info);

    if (!environ_cb) {
        core_log(RETRO_LOG_ERROR, "Environment callback not set");
        return false;
    }

    // Set up OpenGL context
    hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
    hw_render.version_major = 3;
    hw_render.version_minor = 3;
    hw_render.context_reset = module_opengl_init;
    hw_render.context_destroy = module_opengl_deinit;
    hw_render.bottom_left_origin = true;
    hw_render.depth = true;
    hw_render.stencil = false;
    hw_render.cache_context = false;
    hw_render.debug_context = true;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        core_log(RETRO_LOG_ERROR, "Failed to set OpenGL context");
        return false;
    }

    module_opengl_set_callbacks(hw_render.get_proc_address, hw_render.get_current_framebuffer, &use_default_fbo);

    // Process first valid zip file
    bool lua_initialized = false;
    for (size_t i = 0; i < num_info; i++) {
        if (info[i].path) {
            // Store zip file path (use first valid path)
            if (zip_file_path[0] == '\0') {
                strncpy(zip_file_path, info[i].path, sizeof(zip_file_path) - 1);
                zip_file_path[sizeof(zip_file_path) - 1] = '\0';
                core_log(RETRO_LOG_INFO, "Zip file path (special): %s", zip_file_path);
            }

            // Extract and load script.lua
            char *script_data = NULL;
            size_t script_size = 0;
            if (extract_lua_script_from_file(info[i].path, &script_data, &script_size)) {
                if (module_lua_init_from_buffer(script_data, script_size)) {
                    core_log(RETRO_LOG_INFO, "Lua initialized with script from zip (entry %zu)", i);
                    lua_initialized = true;
                } else {
                    core_log(RETRO_LOG_WARN, "Failed to initialize Lua with script from zip (entry %zu)", i);
                }
                free(script_data);
            } else {
                core_log(RETRO_LOG_WARN, "Failed to extract script.lua from zip (entry %zu)", i);
            }
        }
    }

    // Fallback to default script if no valid script was loaded
    if (!lua_initialized) {
        core_log(RETRO_LOG_INFO, "No valid script loaded, using default script");
        zip_file_path[0] = '\0'; // Clear zip path
        if (!module_lua_init()) {
            core_log(RETRO_LOG_WARN, "Failed to initialize Lua with default script");
        }
    }

    core_log(RETRO_LOG_INFO, "Game special loaded");
    return true;
}



// Unload game
void retro_unload_game(void) {
   core_log(RETRO_LOG_INFO, "Game unloaded");
}

// Get region
unsigned retro_get_region(void) {
   core_log(RETRO_LOG_INFO, "Region: NTSC");
   return RETRO_REGION_NTSC;
}

// Stubbed serialization
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void *data, size_t size) { return false; }
size_t retro_serialize_size(void) { return 0; }

// Stubbed cheats
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}

// Stubbed memory
void *retro_get_memory_data(unsigned id) { return NULL; }
size_t retro_get_memory_size(unsigned id) { return 0; }

// API version
unsigned retro_api_version(void) {
   return RETRO_API_VERSION;
}