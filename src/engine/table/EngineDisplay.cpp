// The dot-matrix display. What it shows is a script: a list of steps, each the address of a
// routine and its arguments. A step sets a task going (a routine called every frame until it
// answers zero) and often a drawing to be done once; when the task is done the next step runs.
#include "engine/table/Engine.h"

namespace encore {
namespace {

constexpr u8 kLit = 0xf2, kUnlit = 0x60;
constexpr u16 kRow = 0xa8;  // bytes from one row of dots to the next

}  // namespace

void Engine::compiledPicture(const u8* code, std::size_t size, u16 start, u16 base, int plane) {
  // AL holds a lit dot and AH an unlit one; BX is where the picture goes.
  for (std::size_t at = start; at + 1 < size;) {
    const u8 op = code[at];
    if (op == 0xc3) return;
    if (op != 0x88) break;
    const u8 m = code[at + 1];
    const u8 value = (m & 0x20) ? kUnlit : kLit;
    u16 disp = 0;
    switch (m & 0xdf) {
      case 0x07: at += 2; break;
      case 0x47: disp = static_cast<u16>(static_cast<i8>(code[at + 2])); at += 3; break;
      case 0x87: disp = static_cast<u16>(code[at + 2] | (code[at + 3] << 8)); at += 4; break;
      default: throw DataError("a picture of the display's is not as expected");
    }
    dot(plane, static_cast<u16>(base + disp)) = value;
  }
  throw DataError("a picture of the display's is not as expected");
}

void Engine::fillDisplay(u16 at, u16 width, u16 rows) {
  for (u16 r = 0; r < rows; ++r, at = static_cast<u16>(at + kRow))
    for (u16 x = 0; x < width; ++x) {
      dot(0, static_cast<u16>(at + x)) = kUnlit;
      dot(1, static_cast<u16>(at + x)) = kUnlit;
    }
}

/// The four fonts: their height in rows, and what takes the drawing place back to the top of
/// the next letter (cs:4b5f and its like).
void Engine::setFont(int which) {
  static constexpr u16 kHeight[4] = {0x0d, 0x0b, 0x08, 0x05};
  static constexpr u16 kBack[4] = {0xf77c, 0xf8cc, 0xfac4, 0xfcbc};
  static constexpr u16 kGlyphs[4] = {0x6200, 0x6410, 0x65d0, 0x6710};
  W(0x447f) = kHeight[which];
  W(0x4481) = kBack[which];
  W(0x4483) = A(kGlyphs[which]);
}

/// cs:6ccd: one letter at `at`, which is left where the original leaves DI.
void Engine::drawChar(u8 c, u16& at) {
  if (c == 0x20) {
    at = static_cast<u16>(at - W(0x4481) + 4);
    return;
  }
  c = static_cast<u8>(c - 0x37);
  u16 glyph = c == 0xf3 ? A(0x371c) : static_cast<u16>(W(0x4483) + c * B(0x447f));
  const u16 rows = static_cast<u16>(W(0x447f) - W(0x447b));
  glyph = static_cast<u16>(glyph + W(0x447d));
  // A row is a byte: its bits, from the top one down, are the dots from left to right, which
  // lie alternately in the two planes.
  u16 row = at;
  for (u16 r = 0; r < rows; ++r, row = static_cast<u16>(row + kRow)) {
    const u8 bits = nativeB(static_cast<u16>(glyph + r));
    for (int d = 0; d < 8; ++d)
      dot(d & 1, static_cast<u16>(row + d / 2)) = (bits & (0x80 >> d)) ? kLit : kUnlit;
  }
  at = row;
}

void Engine::drawText(u16 text, u16 at) {
  u8 c = nativeB(text);
  do {
    u16 place = at;
    drawChar(c, place);
    at = static_cast<u16>(at + 4);
    c = nativeB(++text);
  } while (c != 0);
}

void Engine::forgetNumber() {
  for (u16 i = 0; i < 12; ++i) B(0x4495, i) = 0xcc;
  B(0x44a1) = 0;
  W(0x44a2) = 0;
}

/// cs:6c0f: a number of twelve digits, one to a byte. Digits that have not changed since it
/// was last drawn are left alone, as are the zeros in front.
void Engine::drawNumber(u16 digits, u16 at) {
  if (B(0x44a4) == 0xff) {  // set to the left: move back two for each zero in front
    B(0x44a4) = 0;
    u16 scanned = 0;
    while (scanned < 12 && nativeB(static_cast<u16>(digits + scanned++)) == 0) {}
    at = static_cast<u16>(at - scanned * 2);
  }
  for (u16 i = 0; i < 12; ++i) {
    u8 d = nativeB(static_cast<u16>(digits + i));
    bool draw = true;
    if (d == 0 && B(0x44a1) != 0xff) draw = false;
    else if (d != 0) B(0x44a1) = 0xff;
    if (draw) {
      const u8 before = B(0x4495, i);
      B(0x4495, i) = d;
      if (before == d) draw = false;
    }
    B(0x4489, i) = draw ? d : 0xff;
  }
  B(0x44a1) = 0;
  drawCommas(digits, static_cast<u16>(at - W(0x4481) + 0xf6d4));
  for (u16 i = 0; i < 12; ++i) {
    const u8 d = B(0x4489, i);
    if (d == 0xff) {
      at = static_cast<u16>(at + 4);
    } else {
      drawChar(static_cast<u8>(d + 0x37), at);
      at = static_cast<u16>(at + W(0x4481));
    }
  }
}

/// cs:6d6f: the marks between the thousands, as many as the number needs.
void Engine::drawCommas(u16 digits, u16 base) {
  for (u16 groups = 3;; --groups) {
    if (groups == 0) return;
    const u16 pair = static_cast<u16>(nativeW(digits) + nativeB(static_cast<u16>(digits + 2)));
    const u16 sum = static_cast<u16>((nativeW(digits) & 0xff00) | (pair & 0xff));
    digits = static_cast<u16>(digits + 3);
    if (sum == 0) continue;
    const bool erase = sum >= 0x0a00;
    const bool same = groups == W(0x44a2);
    W(0x44a2) = groups;
    if (same) return;
    u16 at = static_cast<u16>(base + 0xa1b - 0xc8);
    for (u16 n = groups; n > 0; --n, at = static_cast<u16>(at - 0x0c)) {
      const u8 v = (n == 1 && erase) ? kUnlit : kLit;
      dot(1, at) = v;
      dot(1, static_cast<u16>(at + kRow)) = v;
      dot(0, static_cast<u16>(at + 1)) = v;
      dot(0, static_cast<u16>(at + kRow)) = v;
    }
    return;
  }
}

/// The routine at the start of the tables' second code segment: the score in its large
/// digits, which are pictures that are code. Only digits that have changed are drawn.
void Engine::drawScore(u16 digits, u16 at) {
  const u16 scoreDigits = digits, scoreAt = at;
  const u16 segment = static_cast<u16>(cs_[static_cast<u16>(F(0x531c) - 2)] | (cs_[static_cast<u16>(F(0x531c) - 1)] << 8));
  const u8* code = &farB(segment, 0);
  u16 count = 0x0c;
  while (nativeB(digits) == 0) {  // not the zeros in front, but always the last digit
    at = static_cast<u16>(at + 4);
    ++digits;
    if (--count == 1) break;
  }
  W(0x45e8) = count;
  W(0x45ea) = at;
  W(0x45ec) = digits;
  for (int plane = 0; plane < 2; ++plane) {
    u16 place = at;
    for (u16 i = 0; i < count; ++i, place = static_cast<u16>(place + 4)) {
      const u8 d = nativeB(static_cast<u16>(digits + i));
      const u8 before = B(0x45da, i);
      if (plane == 1) B(0x45da, i) = d;
      if (d == before) continue;
      const u16 entry = static_cast<u16>(((d + 0x30) & 0xff) * 2);
      const u16 start = plane == 0 ? static_cast<u16>(W(0x5a00, entry) + 0x1a0) : static_cast<u16>(W(0x5c00, entry) + 0xc40);
      compiledPicture(code, 0x1480, start, place, plane);
    }
  }
  // cs:0135 of that segment: the marks between the thousands
  for (u16 groups = 3;; --groups) {
    if (groups == 0) return;
    const u16 sum = static_cast<u16>((nativeW(scoreDigits + (3 - groups) * 3) & 0xff00) |
                                     ((nativeW(scoreDigits + (3 - groups) * 3) + nativeB(static_cast<u16>(scoreDigits + (3 - groups) * 3 + 2))) & 0xff));
    if (sum == 0) continue;
    const bool same = groups == W(0x45e6);
    W(0x45e6) = groups;
    if (same) return;
    u16 mark = static_cast<u16>(scoreAt + 0xa1b - 0xc8);
    for (u16 n = groups; n > 0; --n, mark = static_cast<u16>(mark - 0x0c)) {
      dot(1, mark) = kLit;
      dot(1, static_cast<u16>(mark + kRow)) = kLit;
      dot(0, static_cast<u16>(mark + 1)) = kLit;
      dot(0, static_cast<u16>(mark + kRow)) = kLit;
    }
    return;
  }
}

// ---------------------------------------------------------------------------------------
// the script
// ---------------------------------------------------------------------------------------

/// cs:444c: once a frame: the task, then the drawing the step asked for, if any.
void Engine::displayStep() {
  B(0x3397) = 0;
  si = W(0x33e9);
  call(W(0x33e7));
  W(0x33e9) = si;
  if (si == 0) {
    W(0x33e7) = F(0x535b);
    if (const u16 next = W(0x33e5); next != 0) runStep(next);
    else displaySteady();
    B(0x33f9) = 0xff;
  }
  ax = W(0x36fc);
  di = W(0x36fe);
  call(W(0x36fa));
  W(0x36fa) = F(0x69fc);
}

void Engine::startScript(u16 native) {
  displaySteady();
  B(0x33f9) = 0;
  W(0x36f6) = 0;
  B(0x33e1) = 0xff;
  runStep(native);
}

void Engine::runStep(u16 native) {
  for (u16 i = 0; i < 12; ++i) B(0x45da, i) = 0x12;  // the score is drawn afresh
  W(0x45e6) = 0;
  bx = native;
  call(nativeW(native));
}

void Engine::nextStep(u16 size) {
  bx = static_cast<u16>(bx + size);
  if (nativeW(bx) == 0) bx = 0;
  W(0x33e5) = bx;
}

void Engine::bindDisplay() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  auto wait = [this](u16 frames) {  // the usual task: count down
    W(0x33e9) = frames;
    W(0x33e7) = F(0x5285);
  };

  // --- tasks: SI comes in as what the task left last time, and goes out zero when done
  bind(0x5285, [this] { --si; });
  bind(0x4d92, [this] { si = 1; });  // for ever
  bind(0x531c, [this] {  // the score, once
    drawScore(si, 0xa0);
    for (u16 i = 0; i < 12; ++i) B(0x45da, i) = 0x12;
    W(0x45e6) = 0;
    si = 0;
  });
  bind(0x535b, [this] {  // nothing in particular to show (cs:535b)
    si = 0;
    if (CB(0x3475) == 0) return;
    if (W(0x36f6) == 0x2d0) {
      if (B(0x338a) != 0xff) {
        W(0x36f8) = 0xff;
        B(0x2249) = B(0x2288);
        B(0x2253) = B(0x228f);
        startScript(A(0x18ea));
      }
    } else if (B(0x372c) == 0xff) {
      B(0x372c) = 0;
      startScript(A(0x19d4));
      si = 0;
      return;
    } else if (B(0x3713) == 0xff) {
      startScript(A(0x17c4));
    } else if (B(0x33e0) != 0xff && B(0x33e1) == 0xff) {
      B(0x33e1) = 0;
      runStep(A(0x1acc));
    }
    // cs:5406: has the score passed the best on the table?
    if (B(0x33e0) != 0xff && B(0x33e2) != 0xff && B(0x33f1) != 0xff) {
      for (u16 i = 0; i < 12; ++i) {
        const u8 best = B(0x0016, i), score = B(0x45b6, i);
        if (score < best) break;
        if (score > best) {
          B(0x33f1) = 0xff;
          startScript(A(0x13f7));
          B(0x33e1) = 0xff;
          break;
        }
      }
    }
    ++W(0x36f6);
    drawScore(A(0x45b6), 0xc8);
    si = 0;
  });
  bind(0x5287, [this] {  // waits for Y or N
    si = 1;
    u8 key = B(at::lastKey);
    if (key == 0xff) return;
    B(at::lastKey) = 0xff;
    key = B(0x3654, key);
    if (key == 0) return;
    if (key == 0x59) CB(0x3732) = 0xff;
    CB(0x3195) = 0;
    CB(0x3475) = 0xff;
    si = 0;
  });

  // --- steps
  bind(0x2cf1, [=, this] {  // go on from another place in the script
    bx = arg(2);
    W(0x33e5) = bx;
    call(nativeW(bx));
  });
  bind(0x453a, [=, this] {  // the score
    W(0x33e9) = arg(2);
    W(0x33e7) = F(0x531c);
    nextStep(4);
  });
  bind(0x454a, [=, this] {  // the display flashes, this many frames each way
    const u16 period = arg(2);
    wait(1);
    W(0x3390) = period;
    W(0x3392) = period;
    W(0x3394) = 0xff;
    B(0x3396) = 0xff;
    nextStep(4);
  });
  // a number at a place, in each font, and set to the left
  struct NumberStep { u16 step, draw; bool left; };
  for (const NumberStep n : {NumberStep{0x45f1, 0x458e, false}, {0x461a, 0x45af, false}, {0x4643, 0x456d, false},
                             {0x466c, 0x456d, true}, {0x469b, 0x45af, true}, {0x46ca, 0x45d0, false}}) {
    bind(n.step, [=, this] {
      if (n.left) B(0x44a4) = 0xff;
      wait(1);
      W(0x447d) = 0;
      W(0x4485) = arg(2);
      W(0x4487) = arg(4);
      W(0x36fa) = F(n.draw);
      nextStep(6);
    });
  }
  struct NumberDraw { u16 at; int font; };
  for (const NumberDraw n : {NumberDraw{0x456d, 2}, {0x458e, 1}, {0x45af, 0}, {0x45d0, 3}}) {
    bind(n.at, [=, this] {
      forgetNumber();
      setFont(n.font);
      drawNumber(W(0x4485), W(0x4487));
    });
  }
  bind(0x46f3, [=, this] {  // a sound
    wait(1);
    const u16 record = arg(2);
    sound->effect(nativeB(record), nativeB(static_cast<u16>(record + 1)), static_cast<u8>(arg(4)),
                  static_cast<u8>(nativeB(static_cast<u16>(record + 3)) + 1));
    nextStep(6);
  });
  bind(0x471e, [=, this] {
    wait(1);
    B(0x230a) = static_cast<u8>(arg(2));
    nextStep(4);
  });
  // text at a place, in each font
  struct TextStep { u16 step; int font; };
  for (const TextStep t : {TextStep{0x4b53, 0}, {0x4b94, 1}, {0x4bd5, 2}, {0x4c16, 3}}) {
    bind(t.step, [=, this] {
      wait(1);
      setFont(t.font);
      W(0x447b) = 0;
      W(0x447d) = 0;
      W(0x36fc) = arg(2);
      W(0x36fe) = arg(4);
      W(0x36fa) = F(0x6ca5);
      nextStep(6);
    });
  }
  bind(0x6ca5, [this] { drawText(ax, di); });
  bind(0x4ceb, [=, this] {
    wait(1);
    displaySteady();
    nextStep(4);
  });
  bind(0x4cfd, [=, this] {
    wait(1);
    if (arg(2) != 0) displayNormal();
    else displayInverse();
    nextStep(4);
  });
  bind(0x4d1e, [=, this] {
    wait(1);
    W(0x3551) = arg(2);
    nextStep(4);
  });
  bind(0x4d52, [=, this] {  // wait
    wait(arg(2));
    nextStep(4);
  });
  bind(0x4d62, [=, this] {  // wait, but not long for one player
    wait(B(0x3716) == 1 ? 2 : arg(2));
    nextStep(4);
  });
  bind(0x4d82, [=, this] {
    W(0x33e9) = arg(2);
    W(0x33e7) = F(0x4d92);
    nextStep(4);
  });
  bind(0x4d96, [=, this] {
    W(0x33e9) = arg(2);
    W(0x33e7) = F(0x5287);
    nextStep(4);
  });

  // --- the big letters that run across the display (cs:6e9c). They are pictures that are
  // code, drawn afresh each frame half a dot further left: by turns in each plane.
  bind(0x4519, [=, this] {
    if (W(0x33e9) != 0) B(0x3397) = 0xff;
    W(0x33e7) = F(0x6e9c);
    W(0x33e9) = arg(2);
    nextStep(4);
  });
  bind(0x6e9c, [this] {
    for (int twice = 0; twice < 2; ++twice) {
      if (nativeB(static_cast<u16>(si + 0x14)) == 0xff) {  // the end of the text
        W(0x44a8) += 1;
        si = 0;
        return;
      }
      struct Pass { u16 mask, at, table, base; };
      for (const Pass pass : {Pass{0x6fb0, 0x6fb3, 0x6000, 0x70a0}, Pass{0x6faf, 0x6fb1, 0x5e00, 0x7c10}}) {
        const int plane = CB(pass.mask) == 1 ? 0 : 1;
        u16 place = CW(pass.at);
        for (u16 i = 0; i < 0x15; ++i, place = static_cast<u16>(place + 4)) {
          const u8 c = nativeB(static_cast<u16>(si + i));
          compiledPicture(cs_.data(), cs_.size(), static_cast<u16>(W(pass.table, static_cast<u16>(c * 2)) + A(pass.base)), place, plane);
        }
      }
      CB(0x6faf) ^= 5;
      CB(0x6fb0) ^= 5;
      if (CB(0x6fb0) == 1) CW(0x6fb1) -= 1;
      else CW(0x6fb3) -= 1;
      if (--CB(0x6f97) == 0) {  // eight half-dots: on to the next letter
        CB(0x6f97) = 8;
        ++si;
        CW(0x6fb1) = kRow;
        CW(0x6fb3) = kRow;
      }
      W(0x44a8) = si;
      W(0x44a6) = W(0x44a6);
    }
  });

  // --- an animation from the table's bank of them (cs:6fb5). Before its frames come where to
  // go back to, how many times round, and where the frames end; each frame is how long it
  // stays and, for each plane, a list of dots to light or put out.
  bind(0x44dd, [=, this] {
    if (W(0x33e9) != 0) B(0x3397) = 0xff;
    const u16 bank = S(0x2056), anim = arg(2);
    farW(bank, 0x42c) = 0;
    farW(bank, 0x42a) = 1;
    farW(bank, 0x428) = farW(bank, static_cast<u16>(anim - 4));
    W(0x33e7) = F(0x6fb5);
    W(0x33e9) = anim;
    nextStep(4);
  });
  bind(0x6fb5, [this] {
    const u16 bank = S(0x2056);
    farW(bank, 0x42e) = si;
    if (--farW(bank, 0x42a) != 0) return;
    const u16 entry = farW(bank, 0x42c);
    if (entry == farW(bank, static_cast<u16>(si - 2))) {
      if (--farW(bank, 0x428) == 0) {
        si = 0;
        farW(bank, 0x42c) = 0;
        farW(bank, 0x42a) = 0;
        return;
      }
      farW(bank, 0x42c) = farW(bank, static_cast<u16>(si - 6));
    }
    farW(bank, 0x42c) += 4;
    farW(bank, 0x42a) = farW(bank, static_cast<u16>(entry + si + 2));
    u16 frame = farW(bank, static_cast<u16>(entry + si));
    for (int plane = 0; plane < 2; ++plane) {
      const u16 count = farW(bank, frame);
      frame = static_cast<u16>(frame + 2);
      u16 place = 0xa7;
      for (u16 i = 0; i < count; ++i) {
        const u8 b = farB(bank, frame++);
        place = static_cast<u16>(place + (b >> 1));
        if (b & 1) dot(plane, place) = kLit;
        else if ((b >> 1) != 0x7f) dot(plane, place) = kUnlit;
      }
    }
  });

  // --- a count down from a number of seconds, shown as two digits (cs:47db)
  bind(0x4757, [=, this] {
    B(0x33e4) = 0;
    W(0x33e9) = 1;
    W(0x33e7) = F(0x47db);
    forgetNumber();
    W(0x36f0) = arg(6);
    W(0x36ee) = 1;
    B(0x33f7) = 0;
    nextStep(8);
  });
  bind(0x47bc, [=, this] {
    W(0x33e9) = 1;
    W(0x33e7) = F(0x47db);
    forgetNumber();
    if (B(0x33e4) == 0xff) {
      B(0x33e4) = 0;
      W(0x36f0) = arg(6);
      B(0x36f3) = static_cast<u8>(0x37 + arg(2));
      B(0x36f4) = static_cast<u8>(0x38 + arg(4));
    }
    W(0x36ee) = 1;
    nextStep(8);
  });
  bind(0x47db, [this] {
    if (B(0x33f7) != 0xff && --W(0x36ee) == 0) {
      W(0x36ee) = W(0x36ec);
      if (B(0x36f4) == 0x37 && B(0x36f3) == 0x2a) {  // nought: done
        W(0x33e9) = 0;
        si = 0;
        return;
      }
      if (--B(0x36f4) == 0x36) {
        B(0x36f4) += 0x0a;
        --B(0x36f3);
      }
      if (B(0x36f3) == 0x37) B(0x36f3) = 0x2a;  // no nought in front
      const u16 keep = si;
      call(F(0x2ddc));  // the table's own, each second
      si = keep;
      setFont(1);
      W(0x447b) = 0;
      W(0x447d) = 0;
      W(0x36fc) = A(0x36f3);
      W(0x36fe) = 0x240;
      W(0x36fa) = F(0x6ca5);
      si = 1;
      return;
    }
    W(0x447d) = 0;
    W(0x4485) = W(0x36f0);
    W(0x4487) = 0x158;
    W(0x36fa) = F(0x45af);
    si = 1;
  });

  // --- text that slides in from below, or out upwards
  auto slide = [this] {  // cs:4988
    const i16 row = W(0x370a).s();
    u16 at = 0;
    if (row < 0) {
      W(0x447b) = static_cast<u16>(-row);
      W(0x447d) = static_cast<u16>(-row);
      at = kRow;
    } else if (row > 3) {
      W(0x447b) = static_cast<u16>(row - 3);
      W(0x447d) = 0;
      at = static_cast<u16>((row + 1) * kRow);
    } else {
      W(0x447b) = 0;
      W(0x447d) = 0;
      at = static_cast<u16>(row * kRow + kRow);
    }
    if (W(0x447b) < 0x0d) drawText(W(0x3708), at);
    if (W(0x370a) == W(0x370c)) {
      si = 0;
      W(0x447b) = 0;
      W(0x447d) = 0;
    } else {
      si = 1;
    }
  };
  bind(0x4892, [=, this] {
    W(0x33e7) = F(0x48cc);
    W(0x3708) = arg(2);
    W(0x370c) = arg(4);
    W(0x370a) = 0x10;
    nextStep(6);
  });
  bind(0x48af, [=, this] {
    W(0x33e7) = F(0x4922);
    W(0x3708) = arg(2);
    W(0x370c) = arg(4);
    W(0x370a) = 0xfff3;
    nextStep(6);
  });
  bind(0x48cc, [=, this] {
    --W(0x370a);
    if (W(0x370a).s() <= 2) {
      W(0x3700) = static_cast<u16>((W(0x370a) + 0x0d) * kRow + kRow);
      W(0x3702) = 0x50;
      W(0x3704) = 1;
      fillDisplay(W(0x3700), 0x50, 1);
    }
    setFont(0);
    slide();
  });
  bind(0x4922, [=, this] {
    ++W(0x370a);
    if (W(0x370a).s() >= 1) {
      W(0x3700) = static_cast<u16>((W(0x370a) - 1) * kRow + kRow);
      W(0x3702) = 0x50;
      W(0x3704) = 1;
      fillDisplay(W(0x3700), 0x50, 1);
    }
    setFont(0);
    slide();
  });

  // --- wipes: the display cleared a little each frame
  bind(0x4dbd, [this] {  // at once
    W(0x33e9) = 1;
    W(0x33e7) = F(0x4dd1);
    W(0x33eb) = 0;
    nextStep(2);
  });
  bind(0x4dd1, [this] {
    W(0x33eb) += kRow;
    W(0x3700) = W(0x33eb);
    W(0x3702) = 0x50;
    W(0x3704) = 0x10;
    fillDisplay(W(0x3700), 0x50, 0x10);
    si = 0;
  });
  bind(0x4df0, [this] {  // from the top down
    W(0x33e9) = 1;
    W(0x33e7) = F(0x4e04);
    W(0x33eb) = 0;
    nextStep(2);
  });
  bind(0x4e04, [this] {
    W(0x33eb) += kRow;
    if (W(0x33eb) == 0xb28) {
      si = 0;
      return;
    }
    W(0x3700) = W(0x33eb);
    W(0x3702) = 0x50;
    W(0x3704) = 1;
    fillDisplay(W(0x3700), 0x50, 1);
    si = 1;
  });
  bind(0x4e2d, [this] {  // from the left
    W(0x33e9) = 1;
    W(0x33e7) = F(0x4e42);
    W(0x33eb) = 0xa7;
    nextStep(2);
  });
  bind(0x4e42, [this] {
    W(0x33eb) += 1;
    if (W(0x33eb) == 0xf8) {
      si = 0;
      return;
    }
    W(0x3700) = W(0x33eb);
    W(0x3702) = 1;
    W(0x3704) = 0x10;
    fillDisplay(W(0x3700), 1, 0x10);
    si = 1;
  });
  bind(0x4e6d, [this] {  // as a blind
    W(0x33e9) = 1;
    W(0x33e7) = F(0x4e82);
    W(0x33eb) = 0;
    nextStep(2);
  });
  bind(0x4e82, [this] {
    W(0x33eb) += kRow;
    if (W(0x33eb) == 0x348) {
      si = 0;
      return;
    }
    for (u16 band : {u16{0}, u16{0x2a0}, u16{0x540}, u16{0x7e0}}) {
      W(0x3700) = static_cast<u16>(W(0x33eb) + band);
      W(0x3702) = 0x50;
      W(0x3704) = 1;
      fillDisplay(W(0x3700), 0x50, 1);
    }
    si = 1;
  });
}

}  // namespace encore
