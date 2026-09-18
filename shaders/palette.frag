#version 410 core
// Pass 1: palette lookup. Converts the 8-bit indexed framebuffer to RGB at native resolution.
//
// The palette texture holds one row per scanline, so a screen can change its colours
// part-way down exactly as the original did with its raster interrupt. When the texture
// has a single row, every scanline shares it.
in vec2 vUv;
out vec4 fragColor;
uniform usampler2D uIndices;   // R8UI, native resolution
uniform sampler2D uPalette;    // 256 x rows RGB
void main() {
  ivec2 size = textureSize(uIndices, 0);
  ivec2 p = ivec2(vUv * vec2(size));
  p.y = size.y - 1 - p.y;  // framebuffer rows are top-down
  uint index = texelFetch(uIndices, p, 0).r;
  int rows = textureSize(uPalette, 0).y;
  int row = rows > 1 ? clamp(p.y, 0, rows - 1) : 0;
  fragColor = vec4(texelFetch(uPalette, ivec2(int(index), row), 0).rgb, 1.0);
}
