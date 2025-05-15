#include "module_opengl.h"
#include "font.h"
#include <stdio.h>
#include <string.h>
#include <cglm/cglm.h>

// Framebuffer dimensions
#define HW_WIDTH 512
#define HW_HEIGHT 512

// Global variables
static retro_hw_get_current_framebuffer_t get_current_framebuffer;
static retro_hw_get_proc_address_t get_proc_address;
static GLuint solid_shader_program = 0;
static GLuint text_shader_program = 0;
static GLuint solid_vao, text_vao; // Separate VAOs
static GLuint vbo;
static GLuint font_texture = 0;
static bool gl_initialized = false;
static bool use_default_fbo = false;

// External logging function
extern void core_log(enum retro_log_level level, const char *fmt, ...);

// Solid vertex shader
static const char *solid_vertex_shader_src =
   "#version 330 core\n"
   "layout(location = 0) in vec2 position;\n"
   "uniform mat4 mvp;\n"
   "void main() {\n"
   "   gl_Position = mvp * vec4(position, 0.0, 1.0);\n"
   "}\n";

// Solid fragment shader
static const char *solid_fragment_shader_src =
   "#version 330 core\n"
   "out vec4 frag_color;\n"
   "uniform vec4 color;\n"
   "void main() {\n"
   "   frag_color = color;\n"
   "}\n";

// Text vertex shader
static const char *text_vertex_shader_src =
   "#version 330 core\n"
   "layout(location = 0) in vec2 position;\n"
   "layout(location = 1) in vec2 texcoord;\n"
   "out vec2 v_texcoord;\n"
   "void main() {\n"
   "   gl_Position = vec4(position, 0.0, 1.0);\n"
   "   v_texcoord = texcoord;\n"
   "}\n";

// Text fragment shader
static const char *text_fragment_shader_src =
   "#version 330 core\n"
   "in vec2 v_texcoord;\n"
   "out vec4 frag_color;\n"
   "uniform sampler2D font_texture;\n"
   "uniform vec4 color;\n"
   "void main() {\n"
   "   float alpha = texture(font_texture, v_texcoord).r;\n"
   "   frag_color = vec4(color.rgb, color.a * alpha);\n"
   "}\n";

// Create shader program
static GLuint create_shader_program(const char *vs_src, const char *fs_src, const char *name) {
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

// Create font texture 
static void create_font_texture(void) {
   uint8_t texture_data[760 * 8] = {0};
   const int chars_per_row = 95;
   const int char_width = 8;
   const int char_height = 8;

   for (int c = 0; c < 95; c++) {
      for (int y = 0; y < 8; y++) {
         uint8_t row = font_8x8[c][y];
         for (int x = 0; x < 8; x++) {
            int tex_x = c * char_width + x;
            int tex_y = y;
            texture_data[tex_y * 760 + tex_x] = (row & (1 << (7 - x))) ? 255 : 0;
         }
      }
   }

   glGenTextures(1, &font_texture);
   glBindTexture(GL_TEXTURE_2D, font_texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 760, 8, 0, GL_RED, GL_UNSIGNED_BYTE, texture_data);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glBindTexture(GL_TEXTURE_2D, 0);
   module_opengl_check_error("create_font_texture");

   core_log(RETRO_LOG_INFO, "Font texture created (760x8)");
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

   text_shader_program = create_shader_program(text_vertex_shader_src, text_fragment_shader_src, "Text");
   if (!text_shader_program) {
      core_log(RETRO_LOG_ERROR, "Failed to create text shader program");
      return;
   }

   create_font_texture();

   // Set up VBO
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW); // Max size for text (4 floats)

   // Solid VAO (position only)
   glGenVertexArrays(1, &solid_vao);
   glBindVertexArray(solid_vao);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
   glBindVertexArray(0);

   // Text VAO (position + texcoord)
   glGenVertexArrays(1, &text_vao);
   glBindVertexArray(text_vao);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
   glEnableVertexAttribArray(1);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
   glBindVertexArray(0);

   glBindBuffer(GL_ARRAY_BUFFER, 0);
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
      glDeleteProgram(text_shader_program);
      glDeleteTextures(1, &font_texture);
      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &solid_vao);
      glDeleteVertexArrays(1, &text_vao);
      gl_initialized = false;
      core_log(RETRO_LOG_INFO, "OpenGL deinitialized");
   }
}

void module_opengl_draw_solid_quad(float x, float y, float w, float h,
                                   float rotation, float r, float g, float b, float a,
                                   float vp_width, float vp_height) {
   if (!glIsProgram(solid_shader_program) || !glIsVertexArray(solid_vao) || !glIsBuffer(vbo)) {
      core_log(RETRO_LOG_ERROR, "Invalid GL state in draw_solid_quad");
      return;
   }

   // Define quad vertices as two triangles
   float vertices[] = {
      -w / 2.0f, -h / 2.0f,
       w / 2.0f, -h / 2.0f,
      -w / 2.0f,  h / 2.0f,
       w / 2.0f, -h / 2.0f,
      -w / 2.0f,  h / 2.0f,
       w / 2.0f,  h / 2.0f
   };

   // Log vertices
   core_log(RETRO_LOG_DEBUG, "Quad vertices: BL(%f, %f), BR(%f, %f), TL(%f, %f), TR(%f, %f)",
            vertices[0], vertices[1], vertices[2], vertices[3], vertices[4], vertices[5], vertices[10], vertices[11]);

   mat4 model, view, proj, mvp;
   glm_mat4_identity(model);
   glm_mat4_identity(view);
   glm_mat4_identity(proj);
   glm_translate(model, (vec3){x, y, 0.0f});
   glm_rotate(model, glm_rad(rotation), (vec3){0.0f, 0.0f, 1.0f});
   glm_ortho(-vp_width / 2.0f, vp_width / 2.0f, vp_height / 2.0f, -vp_height / 2.0f, -1.0f, 1.0f, proj);
   glm_mat4_mul(proj, view, mvp);
   glm_mat4_mul(mvp, model, mvp);

   for (int i = 0; i < 6; i++) {
      vec4 v = {vertices[i * 2], vertices[i * 2 + 1], 0.0f, 1.0f};
      vec4 out;
      glm_mat4_mulv(mvp, v, out);
      core_log(RETRO_LOG_DEBUG, "Vertex %d transformed: (%f, %f, %f, %f)", i, out[0], out[1], out[2], out[3]);
   }

   glUseProgram(solid_shader_program);
   glBindVertexArray(solid_vao);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

   GLint mvp_loc = glGetUniformLocation(solid_shader_program, "mvp");
   glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (float *)mvp);

   GLint color_loc = glGetUniformLocation(solid_shader_program, "color");
   glUniform4f(color_loc, r, g, b, a);

   glDrawArrays(GL_TRIANGLES, 0, 6);

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindVertexArray(0);
   glUseProgram(0);
   module_opengl_check_error("draw_solid_quad");

   core_log(RETRO_LOG_DEBUG, "Drew solid quad at (%f, %f), size (%f, %f), rotation %f", x, y, w, h, rotation);
}


void module_opengl_draw_custom_quad(float *vertices, int num_vertices, float x, float y,
                                   float rotation, float r, float g, float b, float a,
                                   float vp_width, float vp_height) {
    if (!glIsProgram(solid_shader_program) || !glIsVertexArray(solid_vao) || !glIsBuffer(vbo)) {
        core_log(RETRO_LOG_ERROR, "Invalid GL state in draw_custom_quad");
        return;
    }

    if (num_vertices != 4) {
        core_log(RETRO_LOG_ERROR, "draw_custom_quad expects exactly 4 vertices, got %d", num_vertices);
        return;
    }

    // Log vertices
    for (int i = 0; i < num_vertices; i++) {
        core_log(RETRO_LOG_DEBUG, "Vertex %d: (%f, %f)", i, vertices[i * 2], vertices[i * 2 + 1]);
    }

    // Define triangles explicitly: 0-1-2 and 1-2-3
    float triangle_vertices[] = {
        vertices[0], vertices[1], // Vertex 0
        vertices[2], vertices[3], // Vertex 1
        vertices[4], vertices[5], // Vertex 2
        vertices[2], vertices[3], // Vertex 1
        vertices[4], vertices[5], // Vertex 2
        vertices[6], vertices[7]  // Vertex 3
    };

    mat4 model, view, proj, mvp;
    glm_mat4_identity(model);
    glm_mat4_identity(view);
    glm_mat4_identity(proj);
    glm_translate(model, (vec3){x, y, 0.0f});
    glm_rotate(model, glm_rad(rotation), (vec3){0.0f, 0.0f, 1.0f});
    glm_ortho(-vp_width / 2.0f, vp_width / 2.0f, vp_height / 2.0f, -vp_height / 2.0f, -1.0f, 1.0f, proj);
    glm_mat4_mul(proj, view, mvp);
    glm_mat4_mul(mvp, model, mvp);

    // Log transformed vertices
    for (int i = 0; i < 6; i++) {
        vec4 v = {triangle_vertices[i * 2], triangle_vertices[i * 2 + 1], 0.0f, 1.0f};
        vec4 out;
        glm_mat4_mulv(mvp, v, out);
        core_log(RETRO_LOG_DEBUG, "Transformed vertex %d: (%f, %f, %f, %f)", i, out[0], out[1], out[2], out[3]);
    }

    glUseProgram(solid_shader_program);
    glBindVertexArray(solid_vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 6 * 2 * sizeof(float), triangle_vertices);

    GLint mvp_loc = glGetUniformLocation(solid_shader_program, "mvp");
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (float *)mvp);

    GLint color_loc = glGetUniformLocation(solid_shader_program, "color");
    glUniform4f(color_loc, r, g, b, a);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    module_opengl_check_error("draw_custom_quad");

    core_log(RETRO_LOG_DEBUG, "Drew custom quad at (%f, %f), vertices=%d, rotation=%f", x, y, num_vertices, rotation);
}



void module_opengl_draw_text(float x, float y, const char *text,
                             float r, float g, float b, float a,
                             float vp_width, float vp_height) {
   if (!glIsProgram(text_shader_program) || !glIsVertexArray(text_vao) || !glIsBuffer(vbo) || !glIsTexture(font_texture)) {
      core_log(RETRO_LOG_ERROR, "Invalid GL state in draw_text: program=%d, vao=%d, vbo=%d, texture=%d",
               glIsProgram(text_shader_program), glIsVertexArray(text_vao), glIsBuffer(vbo), glIsTexture(font_texture));
      return;
   }

   glUseProgram(text_shader_program);
   glBindVertexArray(text_vao);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, font_texture);

   GLint texture_loc = glGetUniformLocation(text_shader_program, "font_texture");
   if (texture_loc == -1) {
      core_log(RETRO_LOG_ERROR, "Invalid uniform location for font_texture");
      return;
   }
   glUniform1i(texture_loc, 0);

   GLint color_loc = glGetUniformLocation(text_shader_program, "color");
   if (color_loc == -1) {
      core_log(RETRO_LOG_ERROR, "Invalid uniform location for color");
      return;
   }
   glUniform4f(color_loc, r, g, b, a);

   float char_width = 8.0f;
   float char_height = 8.0f;
   float atlas_width = 760.0f;
   float atlas_height = 8.0f;

   for (size_t i = 0; text[i]; i++) {
      unsigned char c = text[i];
      if (c < 32 || c > 126) continue;
      int char_index = c - 32;

      float tex_x0 = (char_index * char_width) / atlas_width;
      float tex_x1 = ((char_index + 1) * char_width) / atlas_width;
      float tex_y0 = 0.0f;
      float tex_y1 = 1.0f;

      float px = x + i * char_width - vp_width / 2.0f;
      float py = y - vp_height / 2.0f;
      float px2 = px + char_width;
      float py2 = py + char_height;

      float x0 = px / (vp_width / 2.0f);
      float y0 = -py / (vp_height / 2.0f);
      float x1 = px2 / (vp_width / 2.0f);
      float y1 = -py2 / (vp_height / 2.0f);

      float vertices[] = {
         x0, y0, tex_x0, tex_y0,
         x1, y0, tex_x1, tex_y0,
         x0, y1, tex_x0, tex_y1,
         x1, y1, tex_x1, tex_y1
      };

      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      module_opengl_check_error("draw_text per character");
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindVertexArray(0);
   glUseProgram(0);
   module_opengl_check_error("draw_text");

   core_log(RETRO_LOG_DEBUG, "Drew text '%s' at (%f, %f)", text, x, y);
}

bool module_opengl_bind_framebuffer(void) {
   GLuint fbo = 0;
   if (use_default_fbo || !get_current_framebuffer) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      core_log(RETRO_LOG_INFO, "Using default framebuffer (0)");
   } else {
      fbo = (GLuint)(uintptr_t)(get_current_framebuffer());
      core_log(RETRO_LOG_INFO, "get_current_framebuffer returned FBO: %u", fbo);
      if (fbo == 0) {
         core_log(RETRO_LOG_WARN, "get_current_framebuffer returned 0, falling back to default framebuffer");
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
         use_default_fbo = true;
      } else {
         glBindFramebuffer(GL_FRAMEBUFFER, fbo);
         GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
         if (status != GL_FRAMEBUFFER_COMPLETE) {
            core_log(RETRO_LOG_ERROR, "Framebuffer %u incomplete (status: %d), falling back to default framebuffer", fbo, status);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            use_default_fbo = true;
         } else {
            core_log(RETRO_LOG_INFO, "Successfully bound FBO: %u", fbo);
            return true;
         }
      }
   }
   return false;
}

void module_opengl_set_viewport(void) {
   glViewport(0, 0, HW_WIDTH, HW_HEIGHT);
   core_log(RETRO_LOG_INFO, "Set viewport to %dx%d", HW_WIDTH, HW_HEIGHT);
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