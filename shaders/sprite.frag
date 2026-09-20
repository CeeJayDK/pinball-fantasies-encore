#version 410 core
// Draws a flipper as one picture turned to the angle the table computes, instead of the
// original's twenty-one drawn positions. Runs over the scene, at the window's resolution.
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uSprite;   // the flipper at rest, RGBA, top row first
uniform usampler2D uMap;     // the replacement-picture map: its low bit marks what covers the flipper
uniform vec2 uSpriteSize;    // pixels
uniform vec2 uFrameSize;     // screen pixels
uniform vec2 uPivotFrame;    // the hinge, in screen pixels
uniform vec2 uPivotSprite;   // the hinge, in the picture's pixels
uniform vec2 uScale;         // picture pixels per screen pixel
uniform float uAngle;        // radians away from the resting position
uniform float uTint;         // the table's fade
uniform vec2 uClip;          // the screen rows the flipper may be drawn in

void main() {
  vec2 f = vec2(vUv.x, 1.0 - vUv.y) * uFrameSize;  // screen rows are top-down
  if (f.y < uClip.x || f.y >= uClip.y) discard;    // never over the dot matrix
  ivec2 p = clamp(ivec2(floor(f)), ivec2(0), ivec2(uFrameSize) - 1);
  if ((texelFetch(uMap, p, 0).a & 1u) != 0u) discard;  // the ball passes in front
  vec2 d = f - uPivotFrame;
  float c = cos(uAngle), s = sin(uAngle);
  vec2 sp = vec2(c * d.x + s * d.y, -s * d.x + c * d.y) * uScale + uPivotSprite;
  if (any(lessThan(sp, vec2(0.0))) || any(greaterThanEqual(sp, uSpriteSize))) discard;
  vec4 t = texture(uSprite, sp / uSpriteSize);
  if (t.a < 0.02) discard;
  fragColor = vec4(t.rgb * uTint, t.a);
}
