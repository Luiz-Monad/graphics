/**
 * @author Park DongHa (luncliff@gmail.com)
 */
#include <graphics.h>
#include <spdlog/spdlog.h>
#if __has_include(<EGL/eglext_angle.h>)
#include <EGL/eglext_angle.h>
#endif

class opengl_error_category_t final : public std::error_category {
    const char* name() const noexcept override {
        return "OpenGL";
    }
    std::string message(int ec) const override {
        constexpr auto bufsz = 40;
        char buf[bufsz]{};
        const auto len = snprintf(buf, bufsz, "error %5d(%4x)", ec, ec);
        return {buf, static_cast<size_t>(len)};
    }
};

opengl_error_category_t errors{};
std::error_category& get_opengl_category() noexcept {
    return errors;
};

EGLint report_egl_error(gsl::czstring<> fname, EGLint ec = eglGetError()) {
    spdlog::error("{}: {:#x}", fname, ec);
    return ec;
}

egl_context_t::egl_context_t(EGLDisplay display, EGLContext share_context) noexcept {
    spdlog::debug(__FUNCTION__);
    // remember the EGLDisplay
    this->display = display;
    EGLint version[2]{};
    if (eglInitialize(display, version + 0, version + 1) == false) {
        report_egl_error("eglInitialize", eglGetError());
        return;
    }
    major = gsl::narrow<uint16_t>(version[0]);
    minor = gsl::narrow<uint16_t>(version[1]);
    spdlog::debug("EGLDisplay {} {}.{}", display, major, minor);

    // acquire EGLConfigs
    EGLint num_config = 3;
    if (auto ec = get_configs(configs, num_config, nullptr)) {
        report_egl_error("eglChooseConfig", ec);
        return;
    }

    // create context for OpenGL ES 3.0+
    EGLint attrs[]{EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    if (context = eglCreateContext(display, configs[0], share_context, attrs); context != EGL_NO_CONTEXT)
        spdlog::debug("EGL create: context {} {}", context, share_context);
}

bool egl_context_t::is_valid() const noexcept {
    return context != EGL_NO_CONTEXT;
}

egl_context_t::~egl_context_t() noexcept {
    spdlog::debug(__FUNCTION__);
    destroy();
}

EGLint egl_context_t::resume(gsl::owner<EGLSurface> es_surface, EGLConfig) noexcept {
    spdlog::trace(__FUNCTION__);
    if (context == EGL_NO_CONTEXT)
        return EGL_NOT_INITIALIZED;
    if (es_surface == EGL_NO_SURFACE)
        return GL_INVALID_VALUE;

    surface = es_surface;
    eglQuerySurface(display, surface, EGL_WIDTH, &surface_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &surface_height);
    if (auto ec = eglGetError(); ec != EGL_SUCCESS)
        return report_egl_error("eglQuerySurface", ec);

    spdlog::debug("EGL current: {}/{} {}", surface, surface, context);
    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
        return report_egl_error("eglMakeCurrent", eglGetError());
    return EGL_SUCCESS;
}

EGLint egl_context_t::resume(gsl::not_null<EGLNativeWindowType> window) noexcept {
    spdlog::trace(__FUNCTION__);
    if (context == EGL_NO_CONTEXT)
        return EGL_NOT_INITIALIZED;

    // create surface with the window
    EGLint* attrs = nullptr;
    if (surface = eglCreateWindowSurface(display, configs[0], window, attrs); surface == EGL_NO_SURFACE)
        return eglGetError(); /// @todo the value can be EGL_SUCCESS. Check the available cases

    // query some values for future debugging
    eglQuerySurface(display, surface, EGL_WIDTH, &surface_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &surface_height);
    if (auto ec = eglGetError(); ec != EGL_SUCCESS)
        return report_egl_error("eglQuerySurface", ec);
    spdlog::debug("EGL create: surface {} {} {}", surface, surface_width, surface_height);

    // bind surface and context
    spdlog::debug("EGL current: {}/{} {}", surface, surface, context);
    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
        return report_egl_error("eglMakeCurrent", eglGetError());
    return EGL_SUCCESS;
}

EGLint egl_context_t::suspend() noexcept {
    spdlog::trace(__FUNCTION__);
    if (context == EGL_NO_CONTEXT)
        return EGL_NOT_INITIALIZED;

    // unbind surface. OpenGL ES 3.1 will return true
    spdlog::debug("EGL current: EGL_NO_SURFACE/EGL_NO_SURFACE {}", context);
    if (eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context) == EGL_FALSE) {
        // OpenGL ES 3.0 will report error. consume it
        // then unbind both surface and context.
        report_egl_error("eglMakeCurrent", eglGetError());
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    // destroy known surface
    if (surface != EGL_NO_SURFACE) {
        spdlog::warn("EGL destroy: surface {}", surface);
        if (eglDestroySurface(display, surface) == EGL_FALSE)
            return report_egl_error("eglDestroySurface", eglGetError());
        surface = EGL_NO_SURFACE;
        surface_width = surface_height = 0;
    }
    return EGL_SUCCESS;
}

void egl_context_t::destroy() noexcept {
    spdlog::trace(__FUNCTION__);
    if (display == EGL_NO_DISPLAY) // already terminated
        return;

    // unbind surface and context
    spdlog::debug("EGL current: EGL_NO_SURFACE/EGL_NO_SURFACE EGL_NO_CONTEXT");
    if (eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
        report_egl_error("eglMakeCurrent", eglGetError());
        return;
    }
    // destroy known context
    if (context != EGL_NO_CONTEXT) {
        spdlog::warn("EGL destroy: context {}", context);
        if (eglDestroyContext(display, context) == EGL_FALSE)
            report_egl_error("eglDestroyContext", eglGetError());
        context = EGL_NO_CONTEXT;
    }
    // destroy known surface
    if (surface != EGL_NO_SURFACE) {
        spdlog::warn("EGL destroy: surface {}", surface);
        if (eglDestroySurface(display, surface) == EGL_FALSE)
            report_egl_error("eglDestroySurface", eglGetError());
        surface = EGL_NO_SURFACE;
    }
    // @todo EGLDisplay's lifecycle can be managed outside of this class.
    //       it had better forget about it rather than `eglTerminate`
    //// terminate EGL
    //spdlog::warn("EGL terminate: {}", display);
    //if (eglTerminate(display) == EGL_FALSE)
    //    report_egl_error("eglTerminate", eglGetError());
    display = EGL_NO_DISPLAY;
}

EGLint egl_context_t::swap() noexcept {
    if (eglSwapBuffers(display, surface))
        return EGL_SUCCESS;
    switch (const auto ec = eglGetError()) {
    case EGL_BAD_CONTEXT:
    case EGL_CONTEXT_LOST:
        destroy();
        [[fallthrough]];
    default:
        return ec; // EGL_BAD_SURFACE and the others ...
    }
}

EGLContext egl_context_t::handle() const noexcept {
    return context;
}

EGLint egl_context_t::get_configs(EGLConfig* configs, EGLint& count, const EGLint* attrs) const noexcept {
    constexpr auto color_size = 8;
    constexpr auto depth_size = 16;
    EGLint backup_attrs[]{EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                          EGL_BLUE_SIZE,       color_size,         EGL_GREEN_SIZE,   color_size,
                          EGL_RED_SIZE,        color_size,         EGL_ALPHA_SIZE,   color_size,
                          EGL_DEPTH_SIZE,      depth_size,         EGL_NONE};
    if (attrs == nullptr)
        attrs = backup_attrs;
    if (eglChooseConfig(this->display, attrs, configs, count, &count) == EGL_FALSE)
        return eglGetError();
    return 0;
}

bool for_each_extension(EGLDisplay display, bool (*handler)(std::string_view, void* ptr), void* ptr) noexcept {
    if (const auto txt = eglQueryString(display, EGL_EXTENSIONS)) {
        const auto txtlen = strlen(txt);
        auto offset = 0;
        for (auto i = 0u; i < txtlen; ++i) {
            if (isspace(txt[i]) == false)
                continue;
            if (handler({txt + offset, i - offset}, ptr))
                return true;
            offset = ++i;
        }
    }
    return false;
}

void get_extensions(EGLDisplay display, std::vector<std::string_view>& names) noexcept {
    for_each_extension(
        display,
        [](std::string_view name, void* ptr) {
            auto& ref = *reinterpret_cast<std::vector<std::string_view>*>(ptr);
            ref.emplace_back(name);
            return false; // continue loop
        },
        &names);
}

bool has_extension(EGLDisplay display, std::string_view name) noexcept {
    return for_each_extension(
        display,
        [](std::string_view name, void* ptr) {
            const auto& ref = *reinterpret_cast<std::string_view*>(ptr);
            return ref == name; // if same name, then end the loop.
        },
        &name);
}
