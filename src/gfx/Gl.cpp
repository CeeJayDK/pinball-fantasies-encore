#include "gfx/Gl.h"

#include <SDL3/SDL_video.h>

#include "core/Log.h"

namespace pfr {

#define PFR_GL_DEFINE(type, name) type name = nullptr;
PFR_GL_FUNCTIONS(PFR_GL_DEFINE)
#undef PFR_GL_DEFINE

bool loadGlFunctions() {
  bool ok = true;
#define PFR_GL_LOAD(type, name)                                          \
  name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name));           \
  if (!name) {                                                           \
    log::error("this OpenGL driver has no " #name);                      \
    ok = false;                                                          \
  }
  PFR_GL_FUNCTIONS(PFR_GL_LOAD)
#undef PFR_GL_LOAD
  return ok;
}

}  // namespace pfr
