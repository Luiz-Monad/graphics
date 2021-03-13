/**
 * @author Park DongHa (luncliff@gmail.com)
 */
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <catch2/catch_reporter_sonarqube.hpp>
#define SPDLOG_HEADER_ONLY
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/CGLContext.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLDevice.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

fs::path get_asset_dir() noexcept {
#if defined(ASSET_DIR)
    if (fs::exists(ASSET_DIR))
        return {ASSET_DIR};
#endif
    return fs::current_path();
}

/// @see https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_pixelformats/opengl_pixelformats.html
/// @see https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/UpdatinganApplicationtoSupportOpenGL3/UpdatinganApplicationtoSupportOpenGL3.html#//apple_ref/doc/uid/TP40001987-CH3-SW1
SCENARIO("CGLPixelFormat", "[cgl]") {
    auto check_available = [](const CGLPixelFormatAttribute* attrs) {
        CGLPixelFormatObj pixel_format{};
        GLint num_pixel_format{};
        if (auto ec = CGLChoosePixelFormat(attrs, &pixel_format, &num_pixel_format))
            return ec;
        CAPTURE(num_pixel_format);
        return CGLDestroyPixelFormat(pixel_format);
    };

    std::vector attrs{kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core};
    GIVEN("color 24 depth 16 double accelerated") {
        attrs.push_back(kCGLPFAColorSize);
        attrs.push_back((CGLPixelFormatAttribute)24);
        attrs.push_back(kCGLPFADepthSize);
        attrs.push_back((CGLPixelFormatAttribute)16);
        attrs.push_back(kCGLPFADoubleBuffer);
        attrs.push_back(kCGLPFAAccelerated);
        attrs.push_back((CGLPixelFormatAttribute)NULL);
        REQUIRE(check_available(attrs.data()) == kCGLNoError);
    }
    GIVEN("color 32 double") {
        attrs.push_back(kCGLPFAColorSize);
        attrs.push_back((CGLPixelFormatAttribute)32);
        attrs.push_back(kCGLPFADoubleBuffer);
        attrs.push_back(kCGLPFAAccelerated);
        attrs.push_back((CGLPixelFormatAttribute)NULL);
        REQUIRE(check_available(attrs.data()) == kCGLNoError);
    }
    GIVEN("color 32 offscreen") {
        attrs.push_back(kCGLPFAColorSize);
        attrs.push_back((CGLPixelFormatAttribute)32);
        attrs.push_back(kCGLPFAOffScreen); // kCGLBadAttribute
        attrs.push_back((CGLPixelFormatAttribute)NULL);
        REQUIRE(check_available(attrs.data()) == kCGLBadAttribute);
    }
}

auto make_pixel_format() {
    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAAccelerated,                                                      //
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core, //
        // kCGLPFAOffScreen, // bad attribute
        kCGLPFAColorSize, (CGLPixelFormatAttribute)24, //
        kCGLPFADepthSize, (CGLPixelFormatAttribute)16,
        kCGLPFADoubleBuffer,          //
        (CGLPixelFormatAttribute)NULL //
    };
    CGLPixelFormatObj pixel_format{};
    GLint num_pixel_format{};
    REQUIRE(CGLChoosePixelFormat(attrs, &pixel_format, &num_pixel_format) == 0);
    REQUIRE(num_pixel_format);
    return std::unique_ptr<std::remove_pointer_t<CGLPixelFormatObj>, decltype(&CGLDestroyPixelFormat)>{
        pixel_format, &CGLDestroyPixelFormat};
}

TEST_CASE("CGLContext", "[cgl]") {
    auto pixel_format_owner = make_pixel_format();

    CGLContextObj context{};
    REQUIRE(CGLCreateContext(pixel_format_owner.get(), NULL, &context) == 0);
    REQUIRE(context);

    REQUIRE(CGLSetCurrentContext(context) == 0);
    REQUIRE(CGLSetCurrentContext(NULL) == 0);
    REQUIRE(CGLClearDrawable(context) == 0);
    REQUIRE(CGLDestroyContext(context) == 0);
}
