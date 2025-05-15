#ifndef MODULE_OPENGL_H
#define MODULE_OPENGL_H

#include <libretro.h>
#include <glad/glad.h>

// Set RetroArch HW render callbacks (call before init)
void module_opengl_set_callbacks(retro_hw_get_proc_address_t get_proc_address,
                                retro_hw_get_current_framebuffer_t get_current_framebuffer,
                                bool *use_default_fbo);

// Initialize OpenGL
void module_opengl_init(void);

// Deinitialize OpenGL
void module_opengl_deinit(void);

// Draw a solid quad
void module_opengl_draw_solid_quad(float x, float y, float w, float h,
                                   float r, float g, float b, float a,
                                   float vp_width, float vp_height);

// Bind framebuffer for rendering
bool module_opengl_bind_framebuffer(void);

// Set viewport
void module_opengl_set_viewport(void);

// Clear framebuffer
void module_opengl_clear(void);

// Check OpenGL errors
void module_opengl_check_error(const char *context);

// Get OpenGL initialization status
bool module_opengl_is_initialized(void);

// Draw text
void module_opengl_draw_text(float x, float y, const char *text,
                             float r, float g, float b, float a,
                             float vp_width, float vp_height);

#endif // MODULE_OPENGL_H