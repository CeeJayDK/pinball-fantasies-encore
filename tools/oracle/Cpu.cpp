#include "Cpu.h"

#include <cstdio>

namespace oracle {
namespace {

bool parity(u8 v) {
  v ^= v >> 4;
  v ^= v >> 2;
  v ^= v >> 1;
  return !(v & 1);
}

}  // namespace

std::string Cpu::where() const {
  char b[64];
  std::snprintf(b, sizeof b, "%04x:%04x", s[CS], startIp_);
  return b;
}

std::string Cpu::history() const {
  std::string out;
  char b[16];
  for (int i = 0; i < kHistory; ++i) {
    const u32 h = history_[(historyAt_ + i) % kHistory];
    std::snprintf(b, sizeof b, "%04x:%04x ", h >> 16, h & 0xffff);
    out += b;
  }
  return out;
}

void Cpu::bad(u8 opcode) {
  char b[96];
  std::snprintf(b, sizeof b, "instruction %02x not understood at %s", opcode, where().c_str());
  throw CpuError(b);
}

Cpu::Operand Cpu::modrm(u8 m) {
  Operand o;
  const int mod = m >> 6, rm = m & 7;
  if (mod == 3) {
    o.isReg = true;
    o.reg = rm;
    return o;
  }
  u16 off = 0;
  Seg seg = DS;
  switch (rm) {
    case 0: off = static_cast<u16>(r[BX] + r[SI]); break;
    case 1: off = static_cast<u16>(r[BX] + r[DI]); break;
    case 2: off = static_cast<u16>(r[BP] + r[SI]); seg = SS; break;
    case 3: off = static_cast<u16>(r[BP] + r[DI]); seg = SS; break;
    case 4: off = r[SI]; break;
    case 5: off = r[DI]; break;
    case 6:
      if (mod == 0) off = fetch16();
      else { off = r[BP]; seg = SS; }
      break;
    default: off = r[BX]; break;
  }
  if (mod == 1) off = static_cast<u16>(off + static_cast<i8>(fetch8()));
  else if (mod == 2) off = static_cast<u16>(off + fetch16());
  o.seg = dataSeg(seg);
  o.off = off;
  return o;
}

u8 Cpu::get8(const Operand& o) { return o.isReg ? reg8(o.reg) : read8(o.seg, o.off); }
u16 Cpu::get16(const Operand& o) { return o.isReg ? r[o.reg] : read16(o.seg, o.off); }
void Cpu::set8(const Operand& o, u8 v) { o.isReg ? setReg8(o.reg, v) : write8(o.seg, o.off, v); }
void Cpu::set16(const Operand& o, u16 v) {
  if (o.isReg) r[o.reg] = v;
  else write16(o.seg, o.off, v);
}

void Cpu::setSzp8(u8 v) {
  setFlag(ZF, v == 0);
  setFlag(SF, v & 0x80);
  setFlag(PF, parity(v));
}
void Cpu::setSzp16(u16 v) {
  setFlag(ZF, v == 0);
  setFlag(SF, v & 0x8000);
  setFlag(PF, parity(static_cast<u8>(v)));
}

// op: 0 add, 1 or, 2 adc, 3 sbb, 4 and, 5 sub, 6 xor, 7 cmp
u8 Cpu::alu8(int op, u8 a, u8 b) {
  u32 res = 0;
  const u32 c = flag(CF) ? 1 : 0;
  switch (op) {
    case 0: res = u32{a} + b; break;
    case 2: res = u32{a} + b + c; break;
    case 3: res = u32{a} - b - c; break;
    case 5: case 7: res = u32{a} - b; break;
    case 1: res = a | b; break;
    case 4: res = a & b; break;
    default: res = a ^ b; break;
  }
  const u8 v = static_cast<u8>(res);
  if (op == 1 || op == 4 || op == 6) {
    setFlag(CF, false);
    setFlag(OF, false);
    setFlag(AF, false);
  } else {
    setFlag(CF, res & 0x100);
    setFlag(AF, (a ^ b ^ v) & 0x10);
    const bool sub = op == 3 || op == 5 || op == 7;
    setFlag(OF, sub ? ((a ^ b) & (a ^ v) & 0x80) : (~(a ^ b) & (a ^ v) & 0x80));
  }
  setSzp8(v);
  return v;
}

u16 Cpu::alu16(int op, u16 a, u16 b) {
  u32 res = 0;
  const u32 c = flag(CF) ? 1 : 0;
  switch (op) {
    case 0: res = u32{a} + b; break;
    case 2: res = u32{a} + b + c; break;
    case 3: res = u32{a} - b - c; break;
    case 5: case 7: res = u32{a} - b; break;
    case 1: res = a | b; break;
    case 4: res = a & b; break;
    default: res = a ^ b; break;
  }
  const u16 v = static_cast<u16>(res);
  if (op == 1 || op == 4 || op == 6) {
    setFlag(CF, false);
    setFlag(OF, false);
    setFlag(AF, false);
  } else {
    setFlag(CF, res & 0x10000);
    setFlag(AF, (a ^ b ^ v) & 0x10);
    const bool sub = op == 3 || op == 5 || op == 7;
    setFlag(OF, sub ? ((a ^ b) & (a ^ v) & 0x8000) : (~(a ^ b) & (a ^ v) & 0x8000));
  }
  setSzp16(v);
  return v;
}

u8 Cpu::inc8(u8 v, bool dec) {
  const bool c = flag(CF);
  const u8 res = alu8(dec ? 5 : 0, v, 1);
  setFlag(CF, c);
  return res;
}
u16 Cpu::inc16(u16 v, bool dec) {
  const bool c = flag(CF);
  const u16 res = alu16(dec ? 5 : 0, v, 1);
  setFlag(CF, c);
  return res;
}

// op: 0 rol, 1 ror, 2 rcl, 3 rcr, 4 shl, 5 shr, 6 sal, 7 sar. The 80186 takes the count
// modulo 32.
u8 Cpu::shift8(int op, u8 v, int count) {
  count &= 31;
  if (!count) return v;
  bool c = flag(CF);
  for (int i = 0; i < count; ++i) {
    const u8 before = v;
    switch (op) {
      case 0: c = v & 0x80; v = static_cast<u8>((v << 1) | (c ? 1 : 0)); break;
      case 1: c = v & 1; v = static_cast<u8>((v >> 1) | (c ? 0x80 : 0)); break;
      case 2: { const bool n = v & 0x80; v = static_cast<u8>((v << 1) | (c ? 1 : 0)); c = n; break; }
      case 3: { const bool n = v & 1; v = static_cast<u8>((v >> 1) | (c ? 0x80 : 0)); c = n; break; }
      case 4: case 6: c = v & 0x80; v = static_cast<u8>(v << 1); break;
      case 5: c = v & 1; v = static_cast<u8>(v >> 1); break;
      default: c = v & 1; v = static_cast<u8>((v >> 1) | (v & 0x80)); break;
    }
    // The overflow flag is defined for a count of one; this leaves it as the last step's.
    if (op == 0 || op == 2 || op == 4 || op == 6) setFlag(OF, ((v & 0x80) != 0) != c);
    else if (op == 1 || op == 3) setFlag(OF, ((v ^ (v << 1)) & 0x80) != 0);
    else if (op == 5) setFlag(OF, before & 0x80);
    else setFlag(OF, false);
  }
  setFlag(CF, c);
  if (op >= 4) setSzp8(v);
  return v;
}

u16 Cpu::shift16(int op, u16 v, int count) {
  count &= 31;
  if (!count) return v;
  bool c = flag(CF);
  for (int i = 0; i < count; ++i) {
    const u16 before = v;
    switch (op) {
      case 0: c = v & 0x8000; v = static_cast<u16>((v << 1) | (c ? 1 : 0)); break;
      case 1: c = v & 1; v = static_cast<u16>((v >> 1) | (c ? 0x8000 : 0)); break;
      case 2: { const bool n = v & 0x8000; v = static_cast<u16>((v << 1) | (c ? 1 : 0)); c = n; break; }
      case 3: { const bool n = v & 1; v = static_cast<u16>((v >> 1) | (c ? 0x8000 : 0)); c = n; break; }
      case 4: case 6: c = v & 0x8000; v = static_cast<u16>(v << 1); break;
      case 5: c = v & 1; v = static_cast<u16>(v >> 1); break;
      default: c = v & 1; v = static_cast<u16>((v >> 1) | (v & 0x8000)); break;
    }
    if (op == 0 || op == 2 || op == 4 || op == 6) setFlag(OF, ((v & 0x8000) != 0) != c);
    else if (op == 1 || op == 3) setFlag(OF, ((v ^ (v << 1)) & 0x8000) != 0);
    else if (op == 5) setFlag(OF, before & 0x8000);
    else setFlag(OF, false);
  }
  setFlag(CF, c);
  if (op >= 4) setSzp16(v);
  return v;
}

bool Cpu::condition(int cc) const {
  bool v = false;
  switch (cc >> 1) {
    case 0: v = flag(OF); break;
    case 1: v = flag(CF); break;
    case 2: v = flag(ZF); break;
    case 3: v = flag(CF) || flag(ZF); break;
    case 4: v = flag(SF); break;
    case 5: v = flag(PF); break;
    case 6: v = flag(SF) != flag(OF); break;
    default: v = flag(ZF) || flag(SF) != flag(OF); break;
  }
  return (cc & 1) ? !v : v;
}

void Cpu::string(u8 opcode) {
  const bool word = opcode & 1;
  const u16 delta = static_cast<u16>(flag(DF) ? (word ? -2 : -1) : (word ? 2 : 1));
  const u16 src = dataSeg(DS);
  auto once = [&] {
    switch (opcode & 0xfe) {
      case 0xa4:  // movs
        if (word) write16(s[ES], r[DI], read16(src, r[SI]));
        else write8(s[ES], r[DI], read8(src, r[SI]));
        r[SI] = static_cast<u16>(r[SI] + delta);
        r[DI] = static_cast<u16>(r[DI] + delta);
        break;
      case 0xa6:  // cmps
        if (word) alu16(7, read16(src, r[SI]), read16(s[ES], r[DI]));
        else alu8(7, read8(src, r[SI]), read8(s[ES], r[DI]));
        r[SI] = static_cast<u16>(r[SI] + delta);
        r[DI] = static_cast<u16>(r[DI] + delta);
        break;
      case 0xaa:  // stos
        if (word) write16(s[ES], r[DI], r[AX]);
        else write8(s[ES], r[DI], lo(AX));
        r[DI] = static_cast<u16>(r[DI] + delta);
        break;
      case 0xac:  // lods
        if (word) r[AX] = read16(src, r[SI]);
        else setLo(AX, read8(src, r[SI]));
        r[SI] = static_cast<u16>(r[SI] + delta);
        break;
      case 0xae:  // scas
        if (word) alu16(7, r[AX], read16(s[ES], r[DI]));
        else alu8(7, lo(AX), read8(s[ES], r[DI]));
        r[DI] = static_cast<u16>(r[DI] + delta);
        break;
      case 0x6c:  // ins
        write8(s[ES], r[DI], bus_.portIn(r[DX]));
        if (word) write8(s[ES], static_cast<u16>(r[DI] + 1), bus_.portIn(static_cast<u16>(r[DX] + 1)));
        r[DI] = static_cast<u16>(r[DI] + delta);
        break;
      default:  // 0x6e outs
        bus_.portOut(r[DX], read8(src, r[SI]));
        if (word) bus_.portOut(static_cast<u16>(r[DX] + 1), read8(src, static_cast<u16>(r[SI] + 1)));
        r[SI] = static_cast<u16>(r[SI] + delta);
        break;
    }
  };
  if (!rep_) {
    once();
    return;
  }
  const bool compares = (opcode & 0xfe) == 0xa6 || (opcode & 0xfe) == 0xae;
  while (r[CX]) {
    once();
    --r[CX];
    if (compares && flag(ZF) != (rep_ == 1)) break;
  }
}

void Cpu::raise(u8 number) {
  push(flags);
  push(s[CS]);
  push(ip);
  setFlag(IF, false);
  setFlag(TF, false);
  ip = read16(0, static_cast<u16>(number * 4));
  s[CS] = read16(0, static_cast<u16>(number * 4 + 2));
}

void Cpu::run(std::uint64_t limit) {
  halted = false;
  for (std::uint64_t i = 0; i < limit && !halted; ++i) step();
}

u16 Cpu::callRoutine(u16 seg, u16 off, bool withFlags, std::uint64_t limit) {
  // The return address is one no program is at, which is how the return is recognised.
  constexpr u16 kSeg = 0xffff, kOff = 0xfff0;
  u16 savedR[8], savedS[4];
  for (int i = 0; i < 8; ++i) savedR[i] = r[i];
  for (int i = 0; i < 4; ++i) savedS[i] = s[i];
  const u16 savedIp = ip, savedFlags = flags;
  const bool savedHalted = halted;
  if (withFlags) {
    push(flags);
    setFlag(IF, false);
  }
  push(kSeg);
  push(kOff);
  s[CS] = seg;
  ip = off;
  halted = false;
  std::uint64_t n = 0;
  while (!(s[CS] == kSeg && ip == kOff)) {
    if (++n > limit) throw CpuError("a routine called at " + where() + " did not return");
    if (halted) halted = false;
    step();
  }
  const u16 result = r[AX];
  for (int i = 0; i < 8; ++i) r[i] = savedR[i];
  for (int i = 0; i < 4; ++i) s[i] = savedS[i];
  ip = savedIp;
  flags = savedFlags;
  halted = savedHalted;
  return result;
}

u16 Cpu::callFar(u16 seg, u16 off, std::uint64_t limit) { return callRoutine(seg, off, false, limit); }
u16 Cpu::callInterrupt(u16 seg, u16 off, std::uint64_t limit) { return callRoutine(seg, off, true, limit); }

void Cpu::step() {
  startIp_ = ip;
  if (s[CS] < 0x50) throw CpuError("the program went astray, to " + where());
  if (!watch.empty()) {
    if (auto it = watch.find((u32{s[CS]} << 16) | ip); it != watch.end()) it->second();
  }
  history_[historyAt_] = (u32{s[CS]} << 16) | ip;
  historyAt_ = (historyAt_ + 1) % kHistory;
  segOverride_ = -1;
  rep_ = 0;
  ++executed;
  u8 op = fetch8();
  for (;;) {  // prefixes
    if (op == 0x26) segOverride_ = ES;
    else if (op == 0x2e) segOverride_ = CS;
    else if (op == 0x36) segOverride_ = SS;
    else if (op == 0x3e) segOverride_ = DS;
    else if (op == 0xf3) rep_ = 1;
    else if (op == 0xf2) rep_ = 2;
    else if (op != 0xf0) break;  // lock
    op = fetch8();
  }

  // The eight arithmetic operations in their six forms each.
  if (op < 0x40 && (op & 7) < 6) {
    const int alu = op >> 3;
    switch (op & 7) {
      case 0: { const u8 m = fetch8(); const Operand o = modrm(m); const u8 v = alu8(alu, get8(o), reg8((m >> 3) & 7)); if (alu != 7) set8(o, v); break; }
      case 1: { const u8 m = fetch8(); const Operand o = modrm(m); const u16 v = alu16(alu, get16(o), r[(m >> 3) & 7]); if (alu != 7) set16(o, v); break; }
      case 2: { const u8 m = fetch8(); const Operand o = modrm(m); const int g = (m >> 3) & 7; const u8 v = alu8(alu, reg8(g), get8(o)); if (alu != 7) setReg8(g, v); break; }
      case 3: { const u8 m = fetch8(); const Operand o = modrm(m); const int g = (m >> 3) & 7; const u16 v = alu16(alu, r[g], get16(o)); if (alu != 7) r[g] = v; break; }
      case 4: { const u8 v = alu8(alu, lo(AX), fetch8()); if (alu != 7) setLo(AX, v); break; }
      default: { const u16 v = alu16(alu, r[AX], fetch16()); if (alu != 7) r[AX] = v; break; }
    }
    return;
  }

  switch (op) {
    case 0x06: push(s[ES]); break;
    case 0x07: s[ES] = pop(); break;
    case 0x0e: push(s[CS]); break;
    case 0x16: push(s[SS]); break;
    case 0x17: s[SS] = pop(); break;
    case 0x1e: push(s[DS]); break;
    case 0x1f: s[DS] = pop(); break;
    case 0x27: {  // daa
      u8 al = lo(AX);
      const u8 old = al;
      const bool oldC = flag(CF);
      if ((al & 0x0f) > 9 || flag(AF)) { al = static_cast<u8>(al + 6); setFlag(AF, true); } else setFlag(AF, false);
      if (old > 0x99 || oldC) { al = static_cast<u8>(al + 0x60); setFlag(CF, true); } else setFlag(CF, false);
      setLo(AX, al);
      setSzp8(al);
      break;
    }
    case 0x2f: {  // das
      u8 al = lo(AX);
      const u8 old = al;
      const bool oldC = flag(CF);
      if ((al & 0x0f) > 9 || flag(AF)) { al = static_cast<u8>(al - 6); setFlag(AF, true); } else setFlag(AF, false);
      if (old > 0x99 || oldC) { al = static_cast<u8>(al - 0x60); setFlag(CF, true); } else setFlag(CF, false);
      setLo(AX, al);
      setSzp8(al);
      break;
    }
    case 0x37:  // aaa
      if ((lo(AX) & 0x0f) > 9 || flag(AF)) {
        r[AX] = static_cast<u16>(r[AX] + 0x106);
        setFlag(AF, true);
        setFlag(CF, true);
      } else {
        setFlag(AF, false);
        setFlag(CF, false);
      }
      setLo(AX, lo(AX) & 0x0f);
      break;
    case 0x3f:  // aas
      if ((lo(AX) & 0x0f) > 9 || flag(AF)) {
        r[AX] = static_cast<u16>(r[AX] - 6);
        setHi(AX, static_cast<u8>(hi(AX) - 1));
        setFlag(AF, true);
        setFlag(CF, true);
      } else {
        setFlag(AF, false);
        setFlag(CF, false);
      }
      setLo(AX, lo(AX) & 0x0f);
      break;
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
      r[op & 7] = inc16(r[op & 7], false);
      break;
    case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
      r[op & 7] = inc16(r[op & 7], true);
      break;
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x55: case 0x56: case 0x57:
      push(r[op & 7]);
      break;
    case 0x54: push(r[SP]); break;  // the 80186 and later push the value before the push
    case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
      r[op & 7] = pop();
      break;
    case 0x60: {  // pusha
      const u16 sp = r[SP];
      for (int i = 0; i < 8; ++i) push(i == SP ? sp : r[i]);
      break;
    }
    case 0x61:  // popa
      for (int i = 7; i >= 0; --i) {
        const u16 v = pop();
        if (i != SP) r[i] = v;
      }
      break;
    case 0x68: push(fetch16()); break;
    case 0x6a: push(static_cast<u16>(static_cast<i8>(fetch8()))); break;
    case 0x69: case 0x6b: {  // imul reg, r/m, immediate
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const i32 a = static_cast<i16>(get16(o));
      const i32 b = op == 0x69 ? static_cast<i16>(fetch16()) : static_cast<i8>(fetch8());
      const i32 p = a * b;
      r[(m >> 3) & 7] = static_cast<u16>(p);
      const bool over = p != static_cast<i16>(p);
      setFlag(CF, over);
      setFlag(OF, over);
      break;
    }
    case 0x6c: case 0x6d: case 0x6e: case 0x6f: string(op); break;
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f: {
      const i8 d = static_cast<i8>(fetch8());
      if (condition(op & 15)) ip = static_cast<u16>(ip + d);
      break;
    }
    case 0x80: case 0x82: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const int alu = (m >> 3) & 7;
      const u8 v = alu8(alu, get8(o), fetch8());
      if (alu != 7) set8(o, v);
      break;
    }
    case 0x81: case 0x83: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const int alu = (m >> 3) & 7;
      const u16 a = get16(o);
      const u16 b = op == 0x81 ? fetch16() : static_cast<u16>(static_cast<i8>(fetch8()));
      const u16 v = alu16(alu, a, b);
      if (alu != 7) set16(o, v);
      break;
    }
    case 0x84: { const u8 m = fetch8(); const Operand o = modrm(m); alu8(4, get8(o), reg8((m >> 3) & 7)); break; }
    case 0x85: { const u8 m = fetch8(); const Operand o = modrm(m); alu16(4, get16(o), r[(m >> 3) & 7]); break; }
    case 0x86: { const u8 m = fetch8(); const Operand o = modrm(m); const int g = (m >> 3) & 7; const u8 v = get8(o); set8(o, reg8(g)); setReg8(g, v); break; }
    case 0x87: { const u8 m = fetch8(); const Operand o = modrm(m); const int g = (m >> 3) & 7; const u16 v = get16(o); set16(o, r[g]); r[g] = v; break; }
    case 0x88: { const u8 m = fetch8(); const Operand o = modrm(m); set8(o, reg8((m >> 3) & 7)); break; }
    case 0x89: { const u8 m = fetch8(); const Operand o = modrm(m); set16(o, r[(m >> 3) & 7]); break; }
    case 0x8a: { const u8 m = fetch8(); const Operand o = modrm(m); setReg8((m >> 3) & 7, get8(o)); break; }
    case 0x8b: { const u8 m = fetch8(); const Operand o = modrm(m); r[(m >> 3) & 7] = get16(o); break; }
    case 0x8c: { const u8 m = fetch8(); const Operand o = modrm(m); set16(o, s[(m >> 3) & 3]); break; }
    case 0x8d: { const u8 m = fetch8(); const Operand o = modrm(m); r[(m >> 3) & 7] = o.off; break; }
    case 0x8e: { const u8 m = fetch8(); const Operand o = modrm(m); s[(m >> 3) & 3] = get16(o); break; }
    case 0x8f: { const u8 m = fetch8(); const u16 v = pop(); const Operand o = modrm(m); set16(o, v); break; }
    case 0x90: break;
    case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: {
      const u16 v = r[AX];
      r[AX] = r[op & 7];
      r[op & 7] = v;
      break;
    }
    case 0x98: r[AX] = static_cast<u16>(static_cast<i8>(lo(AX))); break;
    case 0x99: r[DX] = (r[AX] & 0x8000) ? 0xffff : 0; break;
    case 0x9a: {
      const u16 off = fetch16(), seg = fetch16();
      push(s[CS]);
      push(ip);
      s[CS] = seg;
      ip = off;
      break;
    }
    case 0x9b: break;  // wait
    case 0x9c: push(flags); break;
    case 0x9d: flags = static_cast<u16>((pop() & 0x0fd5) | 2); break;
    case 0x9e: flags = static_cast<u16>((flags & 0xff00) | (hi(AX) & 0xd5) | 2); break;
    case 0x9f: setHi(AX, static_cast<u8>(flags)); break;
    case 0xa0: setLo(AX, read8(dataSeg(DS), fetch16())); break;
    case 0xa1: r[AX] = read16(dataSeg(DS), fetch16()); break;
    case 0xa2: write8(dataSeg(DS), fetch16(), lo(AX)); break;
    case 0xa3: write16(dataSeg(DS), fetch16(), r[AX]); break;
    case 0xa4: case 0xa5: case 0xa6: case 0xa7: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
      string(op);
      break;
    case 0xa8: alu8(4, lo(AX), fetch8()); break;
    case 0xa9: alu16(4, r[AX], fetch16()); break;
    case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
      setReg8(op & 7, fetch8());
      break;
    case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
      r[op & 7] = fetch16();
      break;
    case 0xc0: case 0xd0: case 0xd2: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const int count = op == 0xc0 ? fetch8() : op == 0xd0 ? 1 : lo(CX);
      set8(o, shift8((m >> 3) & 7, get8(o), count));
      break;
    }
    case 0xc1: case 0xd1: case 0xd3: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const int count = op == 0xc1 ? fetch8() : op == 0xd1 ? 1 : lo(CX);
      set16(o, shift16((m >> 3) & 7, get16(o), count));
      break;
    }
    case 0xc2: { const u16 n = fetch16(); ip = pop(); r[SP] = static_cast<u16>(r[SP] + n); break; }
    case 0xc3: ip = pop(); break;
    case 0xc4: case 0xc5: {  // les, lds
      const u8 m = fetch8();
      const Operand o = modrm(m);
      if (o.isReg) bad(op);
      r[(m >> 3) & 7] = read16(o.seg, o.off);
      s[op == 0xc4 ? ES : DS] = read16(o.seg, static_cast<u16>(o.off + 2));
      break;
    }
    case 0xc6: { const u8 m = fetch8(); const Operand o = modrm(m); set8(o, fetch8()); break; }
    case 0xc7: { const u8 m = fetch8(); const Operand o = modrm(m); set16(o, fetch16()); break; }
    case 0xc8: {  // enter
      const u16 size = fetch16();
      const int level = fetch8() & 31;
      push(r[BP]);
      const u16 frame = r[SP];
      for (int i = 1; i < level; ++i) {
        r[BP] = static_cast<u16>(r[BP] - 2);
        push(read16(s[SS], r[BP]));
      }
      if (level) push(frame);
      r[BP] = frame;
      r[SP] = static_cast<u16>(r[SP] - size);
      break;
    }
    case 0xc9: r[SP] = r[BP]; r[BP] = pop(); break;
    case 0xca: { const u16 n = fetch16(); ip = pop(); s[CS] = pop(); r[SP] = static_cast<u16>(r[SP] + n); break; }
    case 0xcb: ip = pop(); s[CS] = pop(); break;
    case 0xcc: if (!bus_.interrupt(3)) raise(3); break;
    case 0xcd: { const u8 n = fetch8(); if (!bus_.interrupt(n)) raise(n); break; }
    case 0xcf: ip = pop(); s[CS] = pop(); flags = static_cast<u16>((pop() & 0x0fd5) | 2); break;
    case 0xd4: {  // aam
      const u8 base = fetch8();
      if (!base) bad(op);
      const u8 al = lo(AX);
      setHi(AX, static_cast<u8>(al / base));
      setLo(AX, static_cast<u8>(al % base));
      setSzp8(lo(AX));
      break;
    }
    case 0xd5: {  // aad
      const u8 base = fetch8();
      const u8 al = static_cast<u8>(lo(AX) + hi(AX) * base);
      r[AX] = al;
      setSzp8(al);
      break;
    }
    case 0xd7: setLo(AX, read8(dataSeg(DS), static_cast<u16>(r[BX] + lo(AX)))); break;
    case 0xe0: case 0xe1: case 0xe2: {
      const i8 d = static_cast<i8>(fetch8());
      --r[CX];
      if (r[CX] && (op == 0xe2 || flag(ZF) == (op == 0xe1))) ip = static_cast<u16>(ip + d);
      break;
    }
    case 0xe3: { const i8 d = static_cast<i8>(fetch8()); if (!r[CX]) ip = static_cast<u16>(ip + d); break; }
    case 0xe4: setLo(AX, bus_.portIn(fetch8())); break;
    case 0xe5: { const u16 p = fetch8(); r[AX] = static_cast<u16>(bus_.portIn(p) | (bus_.portIn(static_cast<u16>(p + 1)) << 8)); break; }
    case 0xe6: bus_.portOut(fetch8(), lo(AX)); break;
    case 0xe7: { const u16 p = fetch8(); bus_.portOut(p, lo(AX)); bus_.portOut(static_cast<u16>(p + 1), hi(AX)); break; }
    case 0xe8: { const u16 d = fetch16(); push(ip); ip = static_cast<u16>(ip + d); break; }
    case 0xe9: { const u16 d = fetch16(); ip = static_cast<u16>(ip + d); break; }
    case 0xea: { const u16 off = fetch16(), seg = fetch16(); s[CS] = seg; ip = off; break; }
    case 0xeb: { const i8 d = static_cast<i8>(fetch8()); ip = static_cast<u16>(ip + d); break; }
    case 0xec: setLo(AX, bus_.portIn(r[DX])); break;
    case 0xed: r[AX] = static_cast<u16>(bus_.portIn(r[DX]) | (bus_.portIn(static_cast<u16>(r[DX] + 1)) << 8)); break;
    case 0xee: bus_.portOut(r[DX], lo(AX)); break;
    case 0xef: bus_.portOut(r[DX], lo(AX)); bus_.portOut(static_cast<u16>(r[DX] + 1), hi(AX)); break;
    case 0xf4: halted = true; break;
    case 0xf5: setFlag(CF, !flag(CF)); break;
    case 0xf6: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const u8 v = get8(o);
      switch ((m >> 3) & 7) {
        case 0: case 1: alu8(4, v, fetch8()); break;
        case 2: set8(o, static_cast<u8>(~v)); break;
        case 3: { const u8 n = alu8(5, 0, v); set8(o, n); setFlag(CF, v != 0); break; }
        case 4: { r[AX] = static_cast<u16>(lo(AX) * v); const bool c = hi(AX) != 0; setFlag(CF, c); setFlag(OF, c); break; }
        case 5: { const i16 p = static_cast<i16>(static_cast<i8>(lo(AX)) * static_cast<i8>(v)); r[AX] = static_cast<u16>(p); const bool c = p != static_cast<i8>(p); setFlag(CF, c); setFlag(OF, c); break; }
        case 6: {
          if (!v || r[AX] / v > 0xff) throw CpuError("division overflow at " + where());
          const u16 a = r[AX];
          setLo(AX, static_cast<u8>(a / v));
          setHi(AX, static_cast<u8>(a % v));
          break;
        }
        default: {
          const i16 a = static_cast<i16>(r[AX]);
          const i16 d = static_cast<i8>(v);
          if (!d || a / d > 127 || a / d < -128) throw CpuError("division overflow at " + where());
          setLo(AX, static_cast<u8>(a / d));
          setHi(AX, static_cast<u8>(a % d));
          break;
        }
      }
      break;
    }
    case 0xf7: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const u16 v = get16(o);
      switch ((m >> 3) & 7) {
        case 0: case 1: alu16(4, v, fetch16()); break;
        case 2: set16(o, static_cast<u16>(~v)); break;
        case 3: { const u16 n = alu16(5, 0, v); set16(o, n); setFlag(CF, v != 0); break; }
        case 4: { const u32 p = u32{r[AX]} * v; r[AX] = static_cast<u16>(p); r[DX] = static_cast<u16>(p >> 16); const bool c = r[DX] != 0; setFlag(CF, c); setFlag(OF, c); break; }
        case 5: { const i32 p = i32{static_cast<i16>(r[AX])} * static_cast<i16>(v); r[AX] = static_cast<u16>(p); r[DX] = static_cast<u16>(static_cast<u32>(p) >> 16); const bool c = p != static_cast<i16>(p); setFlag(CF, c); setFlag(OF, c); break; }
        case 6: {
          const u32 a = (u32{r[DX]} << 16) | r[AX];
          if (!v || a / v > 0xffff) throw CpuError("division overflow at " + where());
          r[AX] = static_cast<u16>(a / v);
          r[DX] = static_cast<u16>(a % v);
          break;
        }
        default: {
          const i32 a = static_cast<i32>((u32{r[DX]} << 16) | r[AX]);
          const i32 d = static_cast<i16>(v);
          if (!d || a / d > 32767 || a / d < -32768) throw CpuError("division overflow at " + where());
          r[AX] = static_cast<u16>(a / d);
          r[DX] = static_cast<u16>(a % d);
          break;
        }
      }
      break;
    }
    case 0xf8: setFlag(CF, false); break;
    case 0xf9: setFlag(CF, true); break;
    case 0xfa: setFlag(IF, false); break;
    case 0xfb: setFlag(IF, true); break;
    case 0xfc: setFlag(DF, false); break;
    case 0xfd: setFlag(DF, true); break;
    case 0xfe: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const int sub = (m >> 3) & 7;
      if (sub > 1) bad(op);
      set8(o, inc8(get8(o), sub == 1));
      break;
    }
    case 0xff: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      switch ((m >> 3) & 7) {
        case 0: set16(o, inc16(get16(o), false)); break;
        case 1: set16(o, inc16(get16(o), true)); break;
        case 2: { const u16 t = get16(o); push(ip); ip = t; break; }
        case 3: { if (o.isReg) bad(op); const u16 off = read16(o.seg, o.off), seg = read16(o.seg, static_cast<u16>(o.off + 2)); push(s[CS]); push(ip); s[CS] = seg; ip = off; break; }
        case 4: ip = get16(o); break;
        case 5: { if (o.isReg) bad(op); const u16 off = read16(o.seg, o.off), seg = read16(o.seg, static_cast<u16>(o.off + 2)); s[CS] = seg; ip = off; break; }
        case 6: push(get16(o)); break;
        default: bad(op);
      }
      break;
    }
    default: bad(op);
  }
}

}  // namespace oracle
