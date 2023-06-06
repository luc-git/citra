// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "core/hw/gpu.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/frame_dumper_opengl.h"
#include "video_core/renderer_opengl/gl_driver.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"

namespace Layout {
struct FramebufferLayout;
}

namespace Core {
class System;
}

namespace Frontend {

struct Frame {
    u32 width{};                      /// Width of the frame (to detect resize)
    u32 height{};                     /// Height of the frame
    bool color_reloaded = false;      /// Texture attachment was recreated (ie: resized)
    OpenGL::OGLRenderbuffer color{};  /// Buffer shared between the render/present FBO
    OpenGL::OGLFramebuffer render{};  /// FBO created on the render thread
    OpenGL::OGLFramebuffer present{}; /// FBO created on the present thread
    GLsync render_fence{};            /// Fence created on the render thread
    GLsync present_fence{};           /// Fence created on the presentation thread
};
} // namespace Frontend

namespace OpenGL {

/// Structure used for storing information about the textures for each 3DS screen
struct TextureInfo {
    OGLTexture resource;
    GLsizei width;
    GLsizei height;
    GPU::Regs::PixelFormat format;
    GLenum gl_format;
    GLenum gl_type;
};

/// Structure used for storing information about the display target for each 3DS screen
struct ScreenInfo {
    GLuint display_texture;
    Common::Rectangle<float> display_texcoords;
    TextureInfo texture;
};

class RendererOpenGL : public VideoCore::RendererBase {
public:
    explicit RendererOpenGL(Core::System& system, Frontend::EmuWindow& window,
                            Frontend::EmuWindow* secondary_window);
    ~RendererOpenGL() override;

    [[nodiscard]] VideoCore::RasterizerInterface* Rasterizer() override {
        return rasterizer.get();
    }

    void SwapBuffers() override;
    void TryPresent(int timeout_ms, bool is_secondary) override;
    void PrepareVideoDumping() override;
    void CleanupVideoDumping() override;
    void Sync() override;

private:
    void InitOpenGLObjects();
    void ReloadSampler();
    void ReloadShader();
    void PrepareRendertarget();
    void RenderScreenshot();
    void RenderToMailbox(const Layout::FramebufferLayout& layout,
                         std::unique_ptr<Frontend::TextureMailbox>& mailbox, bool flipped);
    void ConfigureFramebufferTexture(TextureInfo& texture,
                                     const GPU::Regs::FramebufferConfig& framebuffer);
    void DrawScreens(const Layout::FramebufferLayout& layout, bool flipped);
    void ApplySecondLayerOpacity();
    void ResetSecondLayerOpacity();
    void DrawBottomScreen(const Layout::FramebufferLayout& layout,
                          const Common::Rectangle<u32>& bottom_screen);
    void DrawTopScreen(const Layout::FramebufferLayout& layout,
                       const Common::Rectangle<u32>& top_screen);
    void DrawSingleScreen(const ScreenInfo& screen_info, float x, float y, float w, float h,
                          Layout::DisplayOrientation orientation);
    void DrawSingleScreenStereo(const ScreenInfo& screen_info_l, const ScreenInfo& screen_info_r,
                                float x, float y, float w, float h,
                                Layout::DisplayOrientation orientation);

    // Loads framebuffer from emulated memory into the display information structure
    void LoadFBToScreenInfo(const GPU::Regs::FramebufferConfig& framebuffer,
                            ScreenInfo& screen_info, bool right_eye);
    // Fills active OpenGL texture with the given RGB color.
    void LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b, const TextureInfo& texture);

private:
    Driver driver;
    OpenGLState state;
    std::unique_ptr<RasterizerOpenGL> rasterizer;

    // OpenGL object IDs
    OGLVertexArray vertex_array;
    OGLBuffer vertex_buffer;
    OGLProgram shader;
    OGLFramebuffer screenshot_framebuffer;
    OGLSampler filter_sampler;

    /// Display information for top and bottom screens respectively
    std::array<ScreenInfo, 3> screen_infos;

    // Shader uniform location indices
    GLuint uniform_modelview_matrix;
    GLuint uniform_color_texture;
    GLuint uniform_color_texture_r;

    // Shader uniform for Dolphin compatibility
    GLuint uniform_i_resolution;
    GLuint uniform_o_resolution;
    GLuint uniform_layer;

    // Shader attribute input indices
    GLuint attrib_position;
    GLuint attrib_tex_coord;

    FrameDumperOpenGL frame_dumper;
};

} // namespace OpenGL
