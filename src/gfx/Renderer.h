#pragma once
// OpenGL renderer: indexed framebuffer -> palette pass -> replacement pictures (at window
// resolution, when a screen has any) -> post-process or CRT pass -> window.
#include <OpenGL/gl3.h>

#include <filesystem>

#include "gfx/Framebuffer.h"
#include "gfx/HdLayer.h"
#include "gfx/Palette.h"
#include "gfx/ShaderProgram.h"

namespace pfr {

class Renderer {
 public:
  Renderer() = default;
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  bool init(const std::filesystem::path& shaderDir, int frameWidth, int frameHeight, double pixelAspect);
  void resizeSource(int frameWidth, int frameHeight);
  void setPixelAspect(double aspect) { pixelAspect_ = aspect; }
  /// false draws with hard pixel edges, true softens only the edge that straddles two pixels.
  void setSmoothEdges(bool on) { smoothEdges_ = on; }
  /// Presents the picture through the CRT-Lottes shader (shaders/crt-lottes.frag).
  void setCrt(bool on) { crt_ = on; }
  bool crt() const { return crt_; }
  /// A high-resolution replacement for one of the original pictures, RGBA.
  void setHdPicture(HdPicture p, int width, int height, const u8* rgba);
  bool hasHdPictures() const { return hdLoaded_ != 0; }
  void setHdEnabled(bool on) { hdEnabled_ = on; }
  bool hdEnabled() const { return hdEnabled_; }

  /// Uploads a single palette shared by every scanline.
  void setPalette(const Palette& palette);
  /// Uploads one palette per scanline: `rows` blocks of 256 colours.
  void setRowPalettes(const Rgb* colors, int rows);

  /// Draws one frame into a window of `windowWidth` x `windowHeight` drawable pixels.
  /// `hd`: where the frame drew original pictures that have replacements.
  void draw(const Framebuffer& frame, int windowWidth, int windowHeight, double timeSeconds,
            const HdFrame* hd = nullptr);
  void pollShaderReload();
  Rect viewport() const { return viewport_; }

 private:
  struct Target {
    GLuint fbo = 0, tex = 0;
    int w = 0, h = 0;
  };
  static void ensureTarget(Target& t, int w, int h);
  static void deleteTarget(Target& t);
  void drawHd(const HdFrame& hd);
  GLuint vao_ = 0;
  GLuint indexTex_ = 0;
  GLuint paletteTex_ = 0;
  Target scene_;
  Target hdScene_;   ///< window-sized scene for frames with replacement pictures
  int paletteRows_ = 0;
  int frameW_ = 0, frameH_ = 0;
  double pixelAspect_ = 1.0;
  bool smoothEdges_ = true;
  Rect viewport_;
  ShaderProgram palettePass_;
  ShaderProgram postPass_;
  ShaderProgram crtPass_;
  bool crt_ = false;
  bool crtLoaded_ = false;
  ShaderProgram hdPass_;
  bool hdPassLoaded_ = false;
  bool hdEnabled_ = true;
  u16 hdLoaded_ = 0;  ///< bit per picture with a replacement
  std::array<GLuint, HdFrame::kCount> hdTex_{};
  GLuint hdMapTex_ = 0;
  int hdMapW_ = 0, hdMapH_ = 0;
  std::filesystem::path shaderDir_;
};

}  // namespace pfr
