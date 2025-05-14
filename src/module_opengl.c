#include "module_opengl.h"
#include <stdio.h>
#include <string.h>

// Framebuffer dimensions
#define HW_WIDTH 512
#define HW_HEIGHT 512

// Global variables
static retro_hw_get_current_framebuffer_t get_current_framebuffer;
static retro_hw_get_proc_address_t get_proc_address;
static GLuint solid_shader_program = 0;
static GLuint vbo, vao;
static bool gl_initialized = false;
static bool use_default_fbo = false;

// External logging function
extern void core_log(enum retro_log_level level, const char *fmt, ...);

// Shaders (GLSL 330 core)
static const char *solid_vertex_shader_src =
   "#version 330 core\n"
   "layout(location = 0) in vec2 position;\n"
   "void main() {\n"
   "   gl_Position = vec4(position, 0.0, 1.0);\n"
   "}\n";

static const char *solid_fragment_shader_src =
   "#version 330 core\n"
   "out vec4 frag_color;\n"
   "uniform vec4 color;\n"
   "void main() {\n"
   "   frag_color = color;\n"
   "}\n";

// Create shader program
static GLuint create_shader_program(const char *vs_src, const char *fs_src, const char *name) {
   // [Unchanged, same as original]
   GLuint vs = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(vs, 1, &vs_src, NULL);
   glCompileShader(vs);
   GLint success;
   glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetShaderInfoLog(vs, 512, NULL, info_log);
      core_log(RETRO_LOG_ERROR, "%s vertex shader compilation failed: %s", name, info_log);
      return 0;
   }

   GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(fs, 1, &fs_src, NULL);
   glCompileShader(fs);
   glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetShaderInfoLog(fs, 512, NULL, info_log);
      core_log(RETRO_LOG_ERROR, "%s fragment shader compilation failed: %s", name, info_log);
      return 0;
   }

   GLuint program = glCreateProgram();
   glAttachShader(program, vs);
   glAttachShader(program, fs);
   glLinkProgram(program);
   glGetProgramiv(program, GL_LINK_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetProgramInfoLog(program, 512, NULL, info_log);
      core_log(RETRO_LOG_ERROR, "%s shader program linking failed: %s", name, info_log);
      return 0;
   }

   glDeleteShader(vs);
   glDeleteShader(fs);
   core_log(RETRO_LOG_INFO, "%s shader program created successfully", name);
   return program;
}

void module_opengl_set_callbacks(retro_hw_get_proc_address_t proc_address,
                                retro_hw_get_current_framebuffer_t framebuffer_cb,
                                bool *default_fbo) {
   get_proc_address = proc_address;
   get_current_framebuffer = framebuffer_cb;
   use_default_fbo = *default_fbo;
   core_log(RETRO_LOG_DEBUG, "Set OpenGL callbacks: get_proc_address=%p, get_current_framebuffer=%p, use_default_fbo=%d",
            get_proc_address, get_current_framebuffer, use_default_fbo);
}


void module_opengl_init(void) {
   if (gl_initialized) {
      core_log(RETRO_LOG_INFO, "OpenGL already initialized, skipping");
      return;
   }

   if (!get_proc_address) {
      core_log(RETRO_LOG_ERROR, "No get_proc_address callback provided, cannot initialize GLAD");
      return;
   }

   if (!gladLoadGLLoader((GLADloadproc)get_proc_address)) {
      core_log(RETRO_LOG_ERROR, "Failed to initialize GLAD");
      return;
   }

   const char *gl_version = (const char *)glGetString(GL_VERSION);
   if (!gl_version) {
      core_log(RETRO_LOG_ERROR, "Failed to get OpenGL version");
      return;
   }
   core_log(RETRO_LOG_INFO, "OpenGL version: %s", gl_version);

   if (!GLAD_GL_VERSION_3_3) {
      core_log(RETRO_LOG_ERROR, "OpenGL 3.3 core profile not supported");
      return;
   }

   solid_shader_program = create_shader_program(solid_vertex_shader_src, solid_fragment_shader_src, "Solid");
   if (!solid_shader_program) {
      core_log(RETRO_LOG_ERROR, "Failed to create solid shader program");
      return;
   }

   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindVertexArray(0);
   module_opengl_check_error("init_opengl VAO setup");

   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   module_opengl_check_error("init_opengl state setup");

   gl_initialized = true;
   core_log(RETRO_LOG_INFO, "OpenGL initialized successfully");
}

void module_opengl_deinit(void) {
   if (gl_initialized) {
      glDeleteProgram(solid_shader_program);
      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
      gl_initialized = false;
      core_log(RETRO_LOG_INFO, "OpenGL deinitialized");
   }
}

void module_opengl_draw_solid_quad(float x, float y, float w, float h,
                                   float r, float g, float b, float a,
                                   float vp_width, float vp_height) {
   if (!glIsProgram(solid_shader_program) || !glIsVertexArray(vao) || !glIsBuffer(vbo)) {
      // core_log(RETRO_LOG_ERROR, "Invalid GL state in draw_solid_quad");
      return;
   }

   float x0 = (x / vp_width) * 2.0f - 1.0f;
   float y0 = 1.0f - (y / vp_height) * 2.0f;
   float x1 = ((x + w) / vp_width) * 2.0f - 1.0f;
   float y1 = 1.0f - ((y + h) / vp_height) * 2.0f;

   float vertices[] = { x0, y0, x1, y0, x0, y1, x1, y1 };
  //  core_log(RETRO_LOG_DEBUG, "Quad vertices: (%f,%f), (%f,%f), (%f,%f), (%f,%f)",
  //           vertices[0], vertices[1], vertices[2], vertices[3], vertices[4], vertices[5], vertices[6], vertices[7]);

   glUseProgram(solid_shader_program);
   glBindVertexArray(vao);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

   GLint color_loc = glGetUniformLocation(solid_shader_program, "color");
   glUniform4f(color_loc, r, g, b, a);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindVertexArray(0);
   glUseProgram(0);
   module_opengl_check_error("draw_solid_quad");

  //  core_log(RETRO_LOG_DEBUG, "Drew solid quad at (%f, %f), size (%f, %f)", x, y, w, h);
}

bool module_opengl_bind_framebuffer(void) {
   GLuint fbo = 0;
   if (use_default_fbo || !get_current_framebuffer) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      // core_log(RETRO_LOG_INFO, "Using default framebuffer (0)");
   } else {
      fbo = (GLuint)(uintptr_t)(get_current_framebuffer());
      // core_log(RETRO_LOG_INFO, "get_current_framebuffer returned FBO: %u", fbo);
      if (fbo == 0) {
        //  core_log(RETRO_LOG_WARN, "get_current_framebuffer returned 0, falling back to default framebuffer");
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
         use_default_fbo = true;
      } else {
         glBindFramebuffer(GL_FRAMEBUFFER, fbo);
         GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
         if (status != GL_FRAMEBUFFER_COMPLETE) {
            // core_log(RETRO_LOG_ERROR, "Framebuffer %u incomplete (status: %d), falling back to default framebuffer", fbo, status);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            use_default_fbo = true;
         } else {
            // core_log(RETRO_LOG_INFO, "Successfully bound FBO: %u", fbo);
            return true;
         }
      }
   }
   return false;
}

void module_opengl_set_viewport(void) {
   GLint viewport[4];
   glGetIntegerv(GL_VIEWPORT, viewport);
   if (viewport[2] != HW_WIDTH || viewport[3] != HW_HEIGHT) {
      glViewport(0, 0, HW_WIDTH, HW_HEIGHT);
      core_log(RETRO_LOG_INFO, "Set viewport to %dx%d", HW_WIDTH, HW_HEIGHT);
   }
   module_opengl_check_error("glViewport");
}

void module_opengl_clear(void) {
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   module_opengl_check_error("glClear");
}

void module_opengl_check_error(const char *context) {
   GLenum err;
   bool has_error = false;
   while ((err = glGetError()) != GL_NO_ERROR) {
      has_error = true;
      core_log(RETRO_LOG_ERROR, "OpenGL error in %s: %d", context, err);
   }
   if (!has_error)
      core_log(RETRO_LOG_DEBUG, "No OpenGL errors in %s", context);
}

bool module_opengl_is_initialized(void) {
   return gl_initialized;
}