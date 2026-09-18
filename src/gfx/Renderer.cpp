#include "gfx/Renderer.h"

#include <algorithm>
#include <cmath>

#include "core/Log.h"

namespace pfr {

Renderer::~Renderer() {
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (indexTex_) glDeleteTextures(1, &indexTex_);
  if (paletteTex_) glDeleteTextures(1, &paletteTex_);
  if (sceneTex_) glDeleteTextures(1, &sceneTex_);
  if (sceneFbo_) glDeleteFramebuffers(1, &sceneFbo_);
}

bool Renderer::init(const std::filesystem::path& shaderDir, int frameWidth, int frameHeight, double pixelAspect) {
  shaderDir_ = shaderDir;
  pixelAspect_ = pixelAspect;
  glGenVertexArrays(1, &vao_);
  glGenTextures(1, &indexTex_);
  glGenTextures(1, &paletteTex_);
  glBindTexture(GL_TEXTURE_2D, paletteTex_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  resizeSource(frameWidth, frameHeight);
  if (!palettePass_.load(shaderDir / "fullscreen.vert", shaderDir / "palette.frag")) return false;
  if (!postPass_.load(shaderDir / "fullscreen.vert", shaderDir / "post.frag")) return false;
  return true;
}

void Renderer::resizeSource(int frameWidth, int frameHeight) {
  frameW_ = frameWidth;
  frameH_ = frameHeight;
  glBindTexture(GL_TEXTURE_2D, indexTex_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, frameWidth, frameHeight, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);
  ensureSceneTarget(frameWidth, frameHeight);
}

void Renderer::ensureSceneTarget(int w, int h) {
  if (sceneTex_ && sceneW_ == w && sceneH_ == h) return;
  if (!sceneTex_) glGenTextures(1, &sceneTex_);
  if (!sceneFbo_) glGenFramebuffers(1, &sceneFbo_);
  sceneW_ = w;
  sceneH_ = h;
  glBindTexture(GL_TEXTURE_2D, sceneTex_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex_, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) log::error("scene framebuffer incomplete");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::pollShaderReload() {
  palettePass_.reloadIfChanged();
  postPass_.reloadIfChanged();
}

void Renderer::setPalette(const Palette& palette) { setRowPalettes(palette.colors().data(), 1); }

void Renderer::setRowPalettes(const Rgb* colors, int rows) {
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, paletteTex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  if (rows != paletteRows_) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 256, rows, 0, GL_RGB, GL_UNSIGNED_BYTE, colors);
    paletteRows_ = rows;
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, rows, GL_RGB, GL_UNSIGNED_BYTE, colors);
  }
}

void Renderer::draw(const Framebuffer& frame, int windowWidth, int windowHeight, double timeSeconds) {
  if (frame.width() != frameW_ || frame.height() != frameH_) resizeSource(frame.width(), frame.height());
  glBindVertexArray(vao_);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  // Upload the indexed frame and the palette.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, indexTex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameW_, frameH_, GL_RED_INTEGER, GL_UNSIGNED_BYTE, frame.data());
  // Pass 1: palette lookup into the native-resolution scene texture.
  glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
  glViewport(0, 0, sceneW_, sceneH_);
  palettePass_.use();
  glUniform1i(palettePass_.uniform("uIndices"), 0);
  glUniform1i(palettePass_.uniform("uPalette"), 1);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Pass 2: present with aspect-correct letterboxing.
  const double targetAspect = (frameW_ * pixelAspect_) / frameH_;
  int vw = windowWidth, vh = static_cast<int>(std::lround(windowWidth / targetAspect));
  if (vh > windowHeight) { vh = windowHeight; vw = static_cast<int>(std::lround(windowHeight * targetAspect)); }
  viewport_ = {(windowWidth - vw) / 2, (windowHeight - vh) / 2, vw, vh};
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, windowWidth, windowHeight);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(viewport_.x, viewport_.y, viewport_.w, viewport_.h);
  postPass_.use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sceneTex_);
  glUniform1i(postPass_.uniform("uScene"), 0);
  glUniform2f(postPass_.uniform("uSceneSize"), static_cast<float>(sceneW_), static_cast<float>(sceneH_));
  glUniform2f(postPass_.uniform("uOutputSize"), static_cast<float>(vw), static_cast<float>(vh));
  glUniform1f(postPass_.uniform("uTime"), static_cast<float>(timeSeconds));
  glUniform1f(postPass_.uniform("uFilter"), smoothEdges_ ? 1.0f : 0.0f);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
}

}  // namespace pfr
