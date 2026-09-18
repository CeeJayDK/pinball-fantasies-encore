#pragma once
// OpenGL renderer: indexed framebuffer -> palette pass -> post-process pass -> window.
#include <OpenGL/gl3.h>

#include <filesystem>

#include "gfx/Framebuffer.h"
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

  /// Uploads a single palette shared by every scanline.
  void setPalette(const Palette& palette);
  /// Uploads one palette per scanline: `rows` blocks of 256 colours.
  void setRowPalettes(const Rgb* colors, int rows);

  /// Draws one frame into a window of `windowWidth` x `windowHeight` drawable pixels.
  void draw(const Framebuffer& frame, int windowWidth, int windowHeight, double timeSeconds);
  void pollShaderReload();
  Rect viewport() const { return viewport_; }

 private:
  void ensureSceneTarget(int w, int h);
  GLuint vao_ = 0;
  GLuint indexTex_ = 0;
  GLuint paletteTex_ = 0;
  GLuint sceneFbo_ = 0;
  GLuint sceneTex_ = 0;
  int sceneW_ = 0, sceneH_ = 0;
  int paletteRows_ = 0;
  int frameW_ = 0, frameH_ = 0;
  double pixelAspect_ = 1.0;
  bool smoothEdges_ = true;
  Rect viewport_;
  ShaderProgram palettePass_;
  ShaderProgram postPass_;
  std::filesystem::path shaderDir_;
};

}  // namespace pfr
