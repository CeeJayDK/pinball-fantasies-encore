#version 410 core
// Draws a high-resolution replacement over the pixels a screen drew from the original
// picture (see src/gfx/HdLayer.h). Runs once per picture on a scene the size of the window.
in vec2 vUv;
out vec4 fragColor;
uniform usampler2D uMap;     // per screen pixel: x8, y8, picture | flags
uniform sampler2D uPicture;  // the replacement, top row first
uniform uint uId;            // the picture this pass draws
uniform vec2 uSourceSize;    // the original picture's size in pixels
uniform float uFade;         // 1 = as drawn, 0 = all uFadeColor
uniform vec3 uFadeColor;

void main() {
  ivec2 size = textureSize(uMap, 0);
  vec2 f = vec2(vUv.x, 1.0 - vUv.y) * vec2(size);  // screen rows are top-down
  ivec2 p = clamp(ivec2(floor(f)), ivec2(0), size - 1);
  uvec4 m = texelFetch(uMap, p, 0);
  if ((m.b & 0xffu) != uId) discard;
  vec2 step = vec2((m.b & 0x100u) != 0u ? 0.5 : 1.0, (m.b & 0x200u) != 0u ? 0.5 : 1.0);
  vec2 src = vec2(m.rg) / 8.0 + fract(f) * step;
  vec3 c = texture(uPicture, src / uSourceSize).rgb;
  fragColor = vec4(mix(uFadeColor, c, uFade), 1.0);
}
