#version 410 core
// Draws a high-resolution replacement over the pixels a screen drew from the original
// picture (see src/gfx/HdLayer.h). Runs once per picture on a scene the size of the window.
in vec2 vUv;
out vec4 fragColor;
uniform usampler2D uMap;     // per screen pixel: x8, y8, picture | flags
uniform sampler2D uPicture;  // the replacement, top row first
uniform sampler2D uPictureLit;  // the same with every lamp lit, when uHasLit
uniform uint uHasLit;        // the picture comes in an unlit and a lit version
uniform uint uId;            // the picture this pass draws
uniform vec2 uSourceSize;    // the original picture's size in pixels
uniform float uFade;         // 1 = as drawn, 0 = all uFadeColor
uniform vec3 uFadeColor;
uniform float uRowStep;      // not 0: a strip repeated downwards, this many of its rows to a screen row

void main() {
  ivec2 size = textureSize(uMap, 0);
  vec2 f = vec2(vUv.x, 1.0 - vUv.y) * vec2(size);  // screen rows are top-down
  ivec2 p = clamp(ivec2(floor(f)), ivec2(0), size - 1);
  uvec4 m = texelFetch(uMap, p, 0);
  if ((m.b & 0xffu) != uId) discard;
  vec2 step = vec2((m.b & 0x100u) != 0u ? 0.5 : 1.0, (m.b & 0x200u) != 0u ? 0.5 : 1.0);
  if (uHasLit == 0u && (m.b & 0x400u) != 0u) step.y = 0.0;  // one line of the picture, drawn out
  if (uRowStep > 0.0) step.y = uRowStep;
  vec2 src = vec2(m.rg) / 8.0 + fract(f) * step;
  // How fast the picture passes under the screen, for choosing between its smaller copies
  // (mipmaps). Left to the hardware it is read off the neighbouring pixels, and where those
  // belong to another picture -- the playfield's edge against the plunger -- their places are
  // far apart, the smallest copy is taken, and a line in the picture's average colour shows.
  vec2 gx = dFdx(f) * step / uSourceSize, gy = dFdy(f) * step / uSourceSize;
  vec3 c = textureGrad(uPicture, src / uSourceSize, gx, gy).rgb;
  if (uHasLit != 0u) {
    // How lit this spot is, read smoothly between the screen's pixels so that a lamp's edge
    // is a soft line rather than a staircase of the original's pixels. Only pixels of this
    // same picture count, so the blend never reaches across to something else.
    vec2 q = f - 0.5;
    ivec2 b = ivec2(floor(q));
    vec2 t = fract(q);
    float here = float((m.b >> 10u) & 0x3fu);
    float lit[4];
    for (int i = 0; i < 4; ++i) {
      ivec2 at = clamp(b + ivec2(i & 1, i >> 1), ivec2(0), size - 1);
      uint other = texelFetch(uMap, at, 0).b;
      lit[i] = (other & 0xffu) == uId ? float((other >> 10u) & 0x3fu) : here;
    }
    float amount = mix(mix(lit[0], lit[1], t.x), mix(lit[2], lit[3], t.x), t.y) / 63.0;
    c = mix(c, textureGrad(uPictureLit, src / uSourceSize, gx, gy).rgb, amount);
  }
  fragColor = vec4(mix(uFadeColor, c, uFade), 1.0);
}
