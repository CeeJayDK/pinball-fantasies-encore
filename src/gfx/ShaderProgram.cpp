#include "gfx/ShaderProgram.h"

#include <fstream>
#include <sstream>
#include <vector>

#include "core/Log.h"

namespace pfr {
namespace {

std::string readText(const std::filesystem::path& p) {
  std::ifstream in(p);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

GLuint compile(GLenum type, const std::string& source, std::string& error) {
  const GLuint shader = glCreateShader(type);
  const char* src = source.c_str();
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> log(static_cast<std::size_t>(len) + 1);
    glGetShaderInfoLog(shader, len, nullptr, log.data());
    error = log.data();
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

}  // namespace

ShaderProgram::~ShaderProgram() {
  if (program_) glDeleteProgram(program_);
}

bool ShaderProgram::load(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath) {
  vertexPath_ = vertexPath;
  fragmentPath_ = fragmentPath;
  return compileFromFiles();
}

bool ShaderProgram::compileFromFiles() {
  std::error_code ec;
  vertexTime_ = std::filesystem::last_write_time(vertexPath_, ec);
  fragmentTime_ = std::filesystem::last_write_time(fragmentPath_, ec);
  const GLuint vs = compile(GL_VERTEX_SHADER, readText(vertexPath_), error_);
  if (!vs) { log::error("vertex shader " + vertexPath_.string() + ": " + error_); return false; }
  const GLuint fs = compile(GL_FRAGMENT_SHADER, readText(fragmentPath_), error_);
  if (!fs) { glDeleteShader(vs); log::error("fragment shader " + fragmentPath_.string() + ": " + error_); return false; }
  const GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> log(static_cast<std::size_t>(len) + 1);
    glGetProgramInfoLog(prog, len, nullptr, log.data());
    error_ = log.data();
    log::error("link " + fragmentPath_.string() + ": " + error_);
    glDeleteProgram(prog);
    return false;
  }
  if (program_) glDeleteProgram(program_);
  program_ = prog;
  return true;
}

bool ShaderProgram::reloadIfChanged() {
  std::error_code ec;
  const auto vt = std::filesystem::last_write_time(vertexPath_, ec);
  const auto ft = std::filesystem::last_write_time(fragmentPath_, ec);
  if (ec || (vt == vertexTime_ && ft == fragmentTime_)) return false;
  log::info("reloading shader " + fragmentPath_.filename().string());
  return compileFromFiles();
}

void ShaderProgram::use() const { glUseProgram(program_); }

GLint ShaderProgram::uniform(const char* name) const { return glGetUniformLocation(program_, name); }

}  // namespace pfr
