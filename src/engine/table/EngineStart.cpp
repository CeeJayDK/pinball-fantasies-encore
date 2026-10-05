// The program's start-up: the options taken in, tables put in the form the engine uses them
// in, the collision masks' copies laid out in video memory, the flippers made ready.
#include "engine/table/Engine.h"

namespace encore {
namespace {

/// The original's "times five, divided by six", for the slower of the two screen modes.
u16 fiveSixths(u16 v) { return static_cast<u16>((5 * i32{static_cast<i16>(v)}) / 6); }

}  // namespace

void Engine::start(const Options& o, ByteView bestScores) {
  B(0x362e) = o.fiveBalls;
  B(0x362f) = o.lowAngle;
  B(0x3630) = o.scrolling;
  B(0x3631) = o.musicOff;
  B(0x3632) = o.highResolution;
  B(0x3633) = o.mono;
  B(at::highResolution) = o.highResolution ? 0xff : 0;
  if (o.lowAngle) {  // cs:30e9: the table lies flatter: less pull down it on every slope
    const u16 slopes = high() ? kw(0x30f6, 1) : kw(0x30e9, 1);
    for (u16 i = 0, n = kw(0x30f9, 1); i < n; ++i) nativeW(static_cast<u16>(slopes + i * 4 + 2)) -= 3;
  }
  B(0x33dd) = o.fiveBalls ? 5 : 3;
  if (o.musicOff) musicKey();
  W(0x23ac) = o.scrolling < 1 ? 0x14 : o.scrolling == 1 ? 0x0b : 0x09;
  if (o.scrolling < 1) B(0x3553) = 0;

  // cs:69bb: the sines are kept high byte first
  for (u16 i = 0; i < 0x0a00; ++i) {
    const u8 a = B(0x4600, static_cast<u16>(i * 2));
    B(0x4600, static_cast<u16>(i * 2)) = B(0x4600, static_cast<u16>(i * 2 + 1));
    B(0x4600, static_cast<u16>(i * 2 + 1)) = a;
  }
  W(0x44a8) = A(0x44d4);  // cs:6e7c
  W(0x44a6) = 0x2a0;
  CW(0x6fb1) = 0xa8;
  CW(0x6fb3) = 0xa8;
  if (!high()) {  // cs:3106: everything about speed is for 70 frames a second
    for (u16 i = 0; i < 8; ++i) W(0x231b, static_cast<u16>(i * 16 + 6)) = fiveSixths(W(0x231b, static_cast<u16>(i * 16 + 6)));
    for (u16 a = 0x68a2; a <= 0x68ae; a = static_cast<u16>(a + 2)) W(a) = fiveSixths(W(a));
  }
  W(0x33e7) = F(0x535b);  // cs:63c8
  W(0x33e9) = 0;
  startScript(A(0x14b9));
  for (u16 i = 0; i < 12; ++i) B(0x45da, i) = 0x12;
  W(0x45e6) = 0;

  // cs:69cb: the two maps of where the ball is hidden, moved to rows of 42 bytes
  struct Move { u16 segment, from, to; };
  for (const Move m : {Move{0x2f94, 0x580, 0x100}, Move{0x358c, 0x480, 0x000}}) {
    const u16 segment = S(m.segment);
    u16 from = m.from, to = m.to;
    for (u16 row = 0; row < 0x240; ++row, to = static_cast<u16>(to + 2))
      for (u16 b = 0; b < 0x28; ++b) farB(segment, to++) = farB(segment, from++);
  }

  // cs:6515, as far as the logic goes: the picture's colours, a quarter as large as the file
  // has them, which is what the video card takes
  for (u16 strip : {u16{0x5224}, u16{0x5947}, u16{0x617b}, u16{0x6a9c}}) {  // the picture's four strips
    const u16 picture = S(strip);
    u16 at = 0;
    while (at < 0x8000 && !(farB(picture, at) == 'C' && farB(picture, static_cast<u16>(at + 1)) == 'M' &&
                            farB(picture, static_cast<u16>(at + 2)) == 'A' && farB(picture, static_cast<u16>(at + 3)) == 'P'))
      ++at;
    at = static_cast<u16>(at + 8);
    for (u16 i = 0; i < 0x300; ++i) {
      farB(picture, static_cast<u16>(at + i)) >>= 2;
      CB(0x4f59, i) = farB(picture, static_cast<u16>(at + i));  // the last strip's are the ones kept
    }
  }
  scroll();
  W(0x2f00) = high() ? 0x13d : 0x19e;  // cs:66cd: the screen line the display begins at

  if (bestScores.size() >= 0x40)  // cs:635d
    for (u16 i = 0; i < 0x40; ++i) nativeB(static_cast<u16>(kw(0x6377, 1) + i)) = bestScores[i];
  W(0x3314) = A(0x2f2c);

  // cs:5841: the lights' colours: counts of colours become counts of bytes, and the colours
  // go from the file's range to the video card's
  for (u16 at = A(0x1079); nativeB(at) != 0;) {
    ++at;
    const u16 bytes = static_cast<u16>(nativeB(at) * 3);
    nativeB(at++) = static_cast<u8>(bytes);
    for (u16 i = 0; i < bytes; ++i, ++at) nativeB(at) = static_cast<u8>((nativeB(at) * 0xa2) >> 8);
  }
  // cs:627f: the attract show's times, kept as gaps, become times
  for (u16 e = A(0x0f41); nativeW(e) != 0xffff; e = static_cast<u16>(e + 10)) {
    nativeW(static_cast<u16>(e + 4)) += nativeW(static_cast<u16>(e + 2));
    nativeW(static_cast<u16>(e + 6)) += nativeW(static_cast<u16>(e + 4));
  }

  // cs:3ab6: three of the masks are copied into video memory past the picture, a byte to
  // each plane in turn, where the original reads the ramps' slopes and materials from
  auto copyMask = [this](u16 segment, u16 from, u16 to, u16 count) {
    const u16 s = S(segment);
    for (std::size_t plane = 0; plane < 4; ++plane)
      for (u16 i = 0; i < count; ++i) video_[plane][static_cast<u16>(to + i)] = farB(s, static_cast<u16>(from + i * 4 + plane));
  };
  copyMask(0x7734, 0, W(0x239f), 0x13ec);
  W(0x23a1) = static_cast<u16>(W(0x239f) + 0x13ec);
  {  // cs:3b8a
    for (u16 i = 0; i < 0x528; ++i) W(0x248a, static_cast<u16>(i * 2)) = static_cast<u16>(i / 0x37);
    for (auto& plane : video_) std::fill(plane.begin(), plane.begin() + 0x0ad4, u8{0});
    const u16 plunger = S(0x8274);  // cs:3cd4: pictures of the plunger, kept there to draw from
    const u16 each = static_cast<u16>(kw(0x3ce9, 1) >> 2), kept = kw(0x3cef, 1);
    for (std::size_t plane = 0; plane < 4; ++plane)
      for (u16 i = 0; i < each; ++i) video_[plane][static_cast<u16>(kept + i)] = farB(plunger, static_cast<u16>(plane * each + i));
    const u16 first = A(0x6950);
    nativeW(static_cast<u16>(first + 0x3a)) = S(0x4c54);
    nativeW(static_cast<u16>(first + 0x76)) = S(0x4fc2);
    nativeW(static_cast<u16>(first + 0xb2)) = S(0x4eb6);
    if (!high())
      for (u16 f = 0; f < 3; ++f)
        for (u16 field = 0x24; field <= 0x2a; field = static_cast<u16>(field + 2)) {
          const u16 at = static_cast<u16>(first + f * 0x3c + field);
          nativeW(at) = fiveSixths(nativeW(at));
        }
    for (u16 field = 0x6c; field <= 0x72; field = static_cast<u16>(field + 2)) {
      nativeW(static_cast<u16>(first + field)) += 8;
      nativeW(static_cast<u16>(first + 0x3c + field)) += 0x10;
    }
    for (u16 f = first; nativeB(f) != 0; f = static_cast<u16>(f + 0x3c)) {
      auto w = [&](u16 field) { return nativeW(static_cast<u16>(f + field)); };
      nativeB(static_cast<u16>(f + 1)) = 0xff;
      w(0x18) = static_cast<u16>(w(0x18) / 3);
      // cs:3d81: each of its pictures takes in the wall round it, so that putting one into
      // the mask does not rub the wall out
      const u16 pictures = w(0x3a), walls = S(0x3b74);
      const u16 width = static_cast<u16>(w(0x06) * 2), rows = w(0x08);
      const u16 place = static_cast<u16>(w(0x04) * 0x28 + (w(0x02) >> 3));
      u16 to = 0;
      for (u16 picture = 0; picture <= w(0x20); ++picture)
        for (u16 r = 0; r < rows; ++r)
          for (u16 b = 0; b < width; ++b) farB(pictures, to++) |= farB(walls, static_cast<u16>(place + r * 0x28 + b));
    }
    const u16 x = W(at::ballX), y = W(at::ballY);  // cs:3d36 for each, wherever the ball is
    for (u16 f = first; nativeB(f) != 0; f = static_cast<u16>(f + 0x3c)) {
      W(at::ballX) = nativeW(static_cast<u16>(f + 0x0a));
      W(at::ballY) = nativeW(static_cast<u16>(f + 0x0e));
      stampFlippers();
    }
    W(at::ballX) = x;
    W(at::ballY) = y;
  }
  copyMask(0x7cd4, 0, W(0x23a1), 0x13ec);
  W(0x23a3) = static_cast<u16>(W(0x23a1) + 0x13ec);
  copyMask(0x7194, 0x23f0, W(0x23a3), 0x0d84);
  {  // and the playfield's top rows in the four spare bytes at the end of each row of picture
    const u16 s = S(0x7194);
    for (std::size_t plane = 0; plane < 4; ++plane)
      for (u16 k = 0; k < 0x23f; ++k)
        for (u16 j = 0; j < 4; ++j)
          video_[plane][static_cast<u16>(0x0b24 + k * 0x54 + j)] = farB(s, static_cast<u16>((k * 4 + j) * 4 + plane));
  }
  {  // cs:65b1: the plunger's picture at rest, and a clean first row past the picture
    const u16 s = S(0x82e2);
    u16 from = 0, to = kw(0x65db, 1);
    for (u16 row = 0, rows = kw(0x65e3, 1), width = kw(0x65e9, 1); row < rows; ++row, ++to)
      for (u16 px = 0; px < width; ++px) {
        video_[px & 3][to] = farB(s, from++);
        if ((px & 3) == 3) ++to;
      }
    for (auto& plane : video_) std::fill(plane.begin() + 0xc7d4, plane.begin() + 0xc7d4 + 0x150, u8{0});
    B(0x23a6) = 0;
  }
  if (high()) B(at::keys) |= 4;
  B(0x3550) = 0;  // no mouse
  B(0x2f06) = 0;
  B(0x2f08) = 0xff;
  B(0x2f07) = 0;
  B(0x2f29) = 0;

  // cs:3759
  B(0x3389) = 0;
  B(0x230a) = 0;
  B(0x230c) = 0;
  W(0x36fa) = F(0x69fc);
  for (u16 a : {u16{0x3706}, u16{0x3708}, u16{0x370a}, u16{0x370e}, u16{0x447b}, u16{0x447d}, u16{0x4481}, u16{0x36f8}}) W(a) = 0;
  B(0x33cf) = 0;
  B(0x3714) = 0;
  B(0x3715) = 0;
  W(0x36f6) = 0x2ce;
  B(0x44a4) = 0;
  stopBlinks();
  for (u16 i = 0, slots = kw(0x3873, 1); i < slots; ++i) W(0x331b, static_cast<u16>(i * 2)) = F(0x69fc);
  for (u16 i = 0; i < 0x32; ++i) W(0x35ca, static_cast<u16>(i * 2)) = 0;
  if (high()) W(0x36ec) = 0x47;

  call(F(0x0004));  // the table's own
  toAttract();
  W(0x3383) = high() ? 0x103 : 0x171;
  W(0x3718) = 0xffff;
  call(F(0x01cd));  // the table's music while it waits
  scroll();
  // cs:58f5: in the picture's own colours the lights are out
  for (u16 light = 1, lights = kw(0x5912, 1); light <= lights; ++light) {
    B(0x3591, light) = 0;
    u16 record = W(0x12bd, static_cast<u16>((light - 1) * 2));
    u16 colour = static_cast<u16>(nativeB(record++) * 3);
    const u8 count = nativeB(record++);
    for (u8 i = 0; i < count; ++i) CB(0x4f59, colour++) = nativeB(record++) >> 1;
  }
  if (o.mono) {
    // cs:6a2f, cs:69fd: every colour becomes the grey of its three parts
    for (u16 at = A(0x1079), n = 0, records = kw(0x6a34, 1); n < records; ++n) {
      ++at;
      const u16 colours = static_cast<u16>(nativeB(at++) / 3);
      for (u16 i = 0; i < colours; ++i, at = static_cast<u16>(at + 3)) {
        const u8 grey = static_cast<u8>((nativeB(at) + nativeB(static_cast<u16>(at + 1)) + nativeB(static_cast<u16>(at + 2))) / 3);
        nativeB(at) = nativeB(static_cast<u16>(at + 1)) = nativeB(static_cast<u16>(at + 2)) = grey;
      }
    }
    for (u16 i = 0; i < 0x300; i = static_cast<u16>(i + 3)) {
      const u8 grey = static_cast<u8>((CB(0x4f59, i) + CB(0x4f59, static_cast<u16>(i + 1)) + CB(0x4f59, static_cast<u16>(i + 2))) / 3);
      CB(0x4f59, i) = CB(0x4f59, static_cast<u16>(i + 1)) = CB(0x4f59, static_cast<u16>(i + 2)) = grey;
    }
  }
  for (u16 i = 0; i < 0x300; ++i) dac_[i] = CB(0x4f59, i) & 0x3f;  // cs:4f3c, at full brightness
  sound->start();
  B(0x2f2b) = 0xff;
  B(0x2f29) = 0xff;
}

}  // namespace encore
