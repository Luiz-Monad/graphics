/**
 * @author Park DongHa (luncliff@gmail.com)
 */
#pragma once
#include <gsl/gsl>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

std::error_category& get_opengl_category() noexcept;

using readback_callback_t = void (*)(void* user_data, const void* mapping, size_t length);

class opengl_readback_t final {
  private:
    GLuint pbos[2]{};
    const uint16_t capacity = 2;
    uint32_t length = 0; // length of the buffer modification
    GLintptr offset = 0;

  public:
    opengl_readback_t(GLint w, GLint h) noexcept(false);
    ~opengl_readback_t() noexcept(false);

    /// @brief fbo -> pbo[idx]
    GLenum pack(uint16_t idx, GLuint fbo, const GLint frame[4]) noexcept;
    GLenum map_and_invoke(uint16_t idx, readback_callback_t callback, void* user_data) noexcept;
};

/**
 * @brief OpenGL Vertex Array Object + RAII
 */
struct opengl_vao_t final {
    GLuint name{};

  public:
    opengl_vao_t() noexcept;
    ~opengl_vao_t() noexcept;
};

/**
 * @brief OpenGL Shader Program + RAII
 */
struct opengl_shader_program_t final {
    GLuint id{}, vs{}, fs{};

  public:
    opengl_shader_program_t(std::string_view vtxt, //
                            std::string_view ftxt) noexcept(false);
    ~opengl_shader_program_t() noexcept;

    operator bool() const noexcept;
    GLint uniform(gsl::czstring<> name) const noexcept;
    GLint attribute(gsl::czstring<> name) const noexcept;

  private:
    static GLuint create_compile_attach(GLuint program, GLenum shader_type, //
                                        std::string_view code) noexcept(false);
    static bool                           //
    get_shader_info(std::string& message, //
                    GLuint shader, GLenum status_name = GL_COMPILE_STATUS) noexcept;
    static bool                            //
    get_program_info(std::string& message, //
                     GLuint program, GLenum status_name = GL_LINK_STATUS) noexcept;
};

/**
 * @brief OpenGL Texture + RAII
 */
struct opengl_texture_t final {
    GLuint name{};
    GLenum target{};

  public:
    opengl_texture_t() noexcept(false);
    opengl_texture_t(GLuint name, GLenum target) noexcept(false);
    ~opengl_texture_t() noexcept(false);

    operator bool() const noexcept;
    GLenum update(uint16_t width, uint16_t height, const void* ptr) noexcept;
};

/**
 * @brief OpenGL FrameBuffer + RAII
 * @code
 * glBindFramebuffer(GL_FRAMEBUFFER, fb.name);
 * @endcode
 */
struct opengl_framebuffer_t final {
    GLuint name{};
    GLuint buffers[2]{}; // color, depth
  public:
    opengl_framebuffer_t(uint16_t width, uint16_t height) noexcept(false);
    ~opengl_framebuffer_t() noexcept;

    GLenum bind(void* context = nullptr) noexcept;
    GLenum read_pixels(uint16_t width, uint16_t height, void* pixels) noexcept;
};
