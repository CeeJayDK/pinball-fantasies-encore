#include "Machine.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace oracle {
namespace {

constexpr u16 kPsp = 0x0800;

std::vector<u8> slurp(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) throw CpuError("cannot read " + p.string());
  return std::vector<u8>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

u16 le16(const std::vector<u8>& d, std::size_t at) { return static_cast<u16>(d[at] | (d[at + 1] << 8)); }

}  // namespace

Machine::Machine(const std::filesystem::path& gameDir, int table, const Config& config)
    : dir_(gameDir), table_(table), config_(config), mem_(0x100000, 0) {
  const auto file = slurp(dir_ / ("TABLE" + std::to_string(table + 1) + ".PRG"));
  if (file.size() < 0x1c || file[0] != 'M' || file[1] != 'Z') throw CpuError("not a program");
  const std::size_t header = std::size_t{le16(file, 8)} * 16;
  const std::size_t pages = le16(file, 4), last = le16(file, 2);
  const std::size_t size = pages * 512 - (last ? 512 - last : 0);
  base_ = kPsp + 0x10;
  std::memcpy(&mem_[std::size_t{base_} * 16], &file[header], size - header);
  for (int i = 0; i < le16(file, 6); ++i) {
    const std::size_t entry = le16(file, 0x18) + std::size_t(i) * 4;
    const std::size_t at = std::size_t{base_} * 16 + std::size_t{le16(file, entry + 2)} * 16 + le16(file, entry);
    const u16 v = static_cast<u16>(mem_[at] | (mem_[at + 1] << 8));
    mem_[at] = static_cast<u8>(v + base_);
    mem_[at + 1] = static_cast<u8>((v + base_) >> 8);
  }
  // What the program finds below itself: its prefix, and the BIOS's notes.
  mem_[std::size_t{kPsp} * 16] = 0xcd;
  mem_[std::size_t{kPsp} * 16 + 1] = 0x20;
  mem_[0x463] = 0xd4;  // the video card's register port, 0x3d4
  mem_[0x464] = 0x03;
  // The services answered here have vectors too, since the program calls some through them:
  // each points at "int n; iret".
  for (u8 n : {u8{0x10}, u8{0x21}, u8{0x33}, u8{0x65}, u8{0x66}}) {
    const u32 stub = 0xf0000 + n * 4u;
    mem_[stub] = 0xcd;
    mem_[stub + 1] = n;
    mem_[stub + 2] = 0xcf;
    mem_[n * 4u] = static_cast<u8>(n * 4u);
    mem_[n * 4u + 1] = 0;
    mem_[n * 4u + 2] = 0x00;
    mem_[n * 4u + 3] = 0xf0;
  }
  nextFree_ = static_cast<u16>(base_ + (size - header + 15) / 16 + le16(file, 0x0a) + 1);

  cpu.s[Cpu::DS] = cpu.s[Cpu::ES] = kPsp;
  cpu.s[Cpu::SS] = static_cast<u16>(base_ + le16(file, 0x0e));
  cpu.r[Cpu::SP] = le16(file, 0x10);
  cpu.s[Cpu::CS] = static_cast<u16>(base_ + le16(file, 0x16));
  cpu.ip = le16(file, 0x14);
  cpu.flags = 0x202;
}

u8 Machine::read(u32 a) {
  a &= 0xfffff;
  if (a >= 0xa0000 && a < 0xb0000) return vga.read(a - 0xa0000);
  return mem_[a];
}

void Machine::write(u32 a, u8 v) {
  a &= 0xfffff;
  if (a >= 0xa0000 && a < 0xb0000) vga.write(a - 0xa0000, v);
  else mem_[a] = v;
}

u8 Machine::portIn(u16 port) {
  if (port >= 0x3c0 && port <= 0x3df) return vga.portIn(port);
  if (port == 0x60) return keyPort_;
  if (port == 0x61) return 0;
  if (port == 0x21 || port == 0xa1) return 0;
  if (log) std::fprintf(stderr, "[port] in %04x at %s\n", port, cpu.where().c_str());
  return 0xff;
}

void Machine::portOut(u16 port, u8 v) {
  if (port >= 0x3c0 && port <= 0x3df) return vga.portOut(port, v);
  if (port == 0x20 || port == 0x21 || port == 0xa0 || port == 0xa1 || port == 0x61) return;
  if (log) std::fprintf(stderr, "[port] out %04x = %02x at %s\n", port, v, cpu.where().c_str());
}

std::string Machine::string0(u16 seg, u16 off) const {
  std::string s;
  for (int i = 0; i < 128; ++i) {
    const u8 c = peek8(seg, static_cast<u16>(off + i));
    if (!c) break;
    s += static_cast<char>(c);
  }
  return s;
}

bool Machine::interrupt(u8 n) {
  switch (n) {
    case 0x10:
      if (log) std::fprintf(stderr, "[video] ax=%04x bx=%04x\n", cpu.r[Cpu::AX], cpu.r[Cpu::BX]);
      if (cpu.hi(Cpu::AX) == 0) vga.setMode(cpu.lo(Cpu::AX));
      return true;
    case 0x21: dos(); return true;
    case 0x33:  // no mouse
      if (cpu.r[Cpu::AX] == 0) cpu.r[Cpu::AX] = 0;
      return true;
    case 0x65: launcher(); return true;
    case 0x66: driver(); return true;
    default:
      // A vector the program set itself goes through memory.
      if (peek16(0, static_cast<u16>(n * 4)) || peek16(0, static_cast<u16>(n * 4 + 2))) return false;
      throw CpuError("interrupt " + std::to_string(n) + " with nothing to answer it, at " + cpu.where());
  }
}

void Machine::launcher() {
  const u16 ax = cpu.r[Cpu::AX];
  if (log) std::fprintf(stderr, "[launcher] ax=%04x bx=%04x\n", ax, cpu.r[Cpu::BX]);
  switch (ax) {
    case 0x0000:
      cpu.setHi(Cpu::AX, 0);
      cpu.setLo(Cpu::BX, static_cast<u8>(table_ + 1));
      break;
    case 0x0200:
      for (int i = 0; i < 6; ++i) write(u32{cpu.s[Cpu::ES]} * 16 + static_cast<u16>(cpu.r[Cpu::BX] + i), config_.bytes[i]);
      break;
    case 0x0100:
      for (int i = 0; i < 6; ++i) config_.bytes[i] = peek8(cpu.s[Cpu::ES], static_cast<u16>(cpu.r[Cpu::BX] + i));
      break;
    case 0x0012: cpu.setLo(Cpu::AX, 0); break;
    case 0xffff: break;
    default: throw CpuError("launcher service " + std::to_string(ax) + " at " + cpu.where());
  }
}

void Machine::dos() {
  const u8 ah = cpu.hi(Cpu::AX);
  if (log) std::fprintf(stderr, "[dos] ax=%04x bx=%04x cx=%04x dx=%04x at %s\n", cpu.r[Cpu::AX], cpu.r[Cpu::BX],
                        cpu.r[Cpu::CX], cpu.r[Cpu::DX], cpu.where().c_str());
  switch (ah) {
    case 0x09: {  // print a string ending in $
      std::string s;
      for (int i = 0; i < 400; ++i) {
        const u8 c = peek8(cpu.s[Cpu::DS], static_cast<u16>(cpu.r[Cpu::DX] + i));
        if (c == '$') break;
        s += static_cast<char>(c);
      }
      std::fprintf(stderr, "[the program says] %s\n", s.c_str());
      break;
    }
    case 0x25:  // set a vector
      mem_[cpu.lo(Cpu::AX) * 4u] = static_cast<u8>(cpu.r[Cpu::DX]);
      mem_[cpu.lo(Cpu::AX) * 4u + 1] = static_cast<u8>(cpu.r[Cpu::DX] >> 8);
      mem_[cpu.lo(Cpu::AX) * 4u + 2] = static_cast<u8>(cpu.s[Cpu::DS]);
      mem_[cpu.lo(Cpu::AX) * 4u + 3] = static_cast<u8>(cpu.s[Cpu::DS] >> 8);
      break;
    case 0x35:  // get a vector
      cpu.r[Cpu::BX] = peek16(0, static_cast<u16>(cpu.lo(Cpu::AX) * 4));
      cpu.s[Cpu::ES] = peek16(0, static_cast<u16>(cpu.lo(Cpu::AX) * 4 + 2));
      break;
    case 0x30: cpu.r[Cpu::AX] = 0x0005; break;  // DOS 5.0
    case 0x3c: case 0x3d: {  // create, open
      const std::string name = string0(cpu.s[Cpu::DS], cpu.r[Cpu::DX]);
      File f;
      f.path = dir_ / name;
      for (const auto& e : std::filesystem::directory_iterator(dir_)) {
        std::string a = e.path().filename().string(), b = name;
        std::transform(a.begin(), a.end(), a.begin(), ::toupper);
        std::transform(b.begin(), b.end(), b.begin(), ::toupper);
        if (a == b) f.path = e.path();
      }
      if (ah == 0x3d) {
        std::ifstream in(f.path, std::ios::binary);
        if (!in) {
          if (log) std::fprintf(stderr, "[dos] no file %s\n", name.c_str());
          cpu.r[Cpu::AX] = 2;
          setCarry(true);
          break;
        }
        f.data.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      }
      if (log) std::fprintf(stderr, "[dos] %s %s\n", ah == 0x3d ? "open" : "create", name.c_str());
      cpu.r[Cpu::AX] = nextHandle_;
      files_[nextHandle_++] = std::move(f);
      setCarry(false);
      break;
    }
    case 0x3e: files_.erase(cpu.r[Cpu::BX]); setCarry(false); break;  // close; nothing is ever written to disk
    case 0x3f: {  // read
      auto it = files_.find(cpu.r[Cpu::BX]);
      if (it == files_.end()) { cpu.r[Cpu::AX] = 6; setCarry(true); break; }
      File& f = it->second;
      const std::size_t n = std::min<std::size_t>(cpu.r[Cpu::CX], f.data.size() - f.at);
      for (std::size_t i = 0; i < n; ++i) write(u32{cpu.s[Cpu::DS]} * 16 + static_cast<u16>(cpu.r[Cpu::DX] + i), f.data[f.at + i]);
      f.at += n;
      cpu.r[Cpu::AX] = static_cast<u16>(n);
      setCarry(false);
      break;
    }
    case 0x40: {  // write
      auto it = files_.find(cpu.r[Cpu::BX]);
      if (it != files_.end()) it->second.written = true;
      cpu.r[Cpu::AX] = cpu.r[Cpu::CX];
      setCarry(false);
      break;
    }
    case 0x42: {  // seek
      auto it = files_.find(cpu.r[Cpu::BX]);
      if (it == files_.end()) { cpu.r[Cpu::AX] = 6; setCarry(true); break; }
      File& f = it->second;
      const long off = static_cast<i32>((u32{cpu.r[Cpu::CX]} << 16) | cpu.r[Cpu::DX]);
      const long from = cpu.lo(Cpu::AX) == 0 ? 0 : cpu.lo(Cpu::AX) == 1 ? static_cast<long>(f.at) : static_cast<long>(f.data.size());
      f.at = static_cast<std::size_t>(std::clamp<long>(from + off, 0, static_cast<long>(f.data.size())));
      cpu.r[Cpu::AX] = static_cast<u16>(f.at);
      cpu.r[Cpu::DX] = static_cast<u16>(f.at >> 16);
      setCarry(false);
      break;
    }
    case 0x48: {  // allocate
      const u16 want = cpu.r[Cpu::BX];
      if (u32{nextFree_} + want + 1 > 0xa000) {
        cpu.r[Cpu::AX] = 8;
        cpu.r[Cpu::BX] = static_cast<u16>(0xa000 - nextFree_ - 1);
        setCarry(true);
        break;
      }
      cpu.r[Cpu::AX] = nextFree_;
      nextFree_ = static_cast<u16>(nextFree_ + want + 1);
      setCarry(false);
      break;
    }
    case 0x49: case 0x4a: setCarry(false); break;  // free, resize
    case 0x4b:  // run a program: the sound driver, which is done here by hand instead
      if (log) std::fprintf(stderr, "[dos] run %s\n", string0(cpu.s[Cpu::DS], cpu.r[Cpu::DX]).c_str());
      setCarry(false);
      break;
    case 0x4c:
      if (log) std::fprintf(stderr, "[dos] exit %d\n", cpu.lo(Cpu::AX));
      exited_ = true;
      cpu.halted = true;
      break;
    default:
      throw CpuError("DOS call " + std::to_string(ah) + " at " + cpu.where());
  }
}

void Machine::callBack(const Callback& c) {
  // As the driver's interrupt does it: one word pushed, zero when nothing was missed.
  cpu.push(0);
  cpu.callFar(c.seg, c.off);
  cpu.pop();
}

void Machine::driver() {
  const u8 al = cpu.lo(Cpu::AX);
  if (log && al != 0x08 && al != 0x16)
    std::fprintf(stderr, "[driver] al=%02x bx=%04x cx=%04x dx=%04x es=%04x at %s\n", al, cpu.r[Cpu::BX], cpu.r[Cpu::CX],
                 cpu.r[Cpu::DX], cpu.s[Cpu::ES], cpu.where().c_str());
  bool plainReturn = true;
  switch (al) {
    case 0x00:  // unload
      haveFrameCallback_ = false;
      lineCallbacks_.clear();
      break;
    case 0x04: driverStatus_ = 0; break;  // start the music: "ok"
    case 0x08:  // poll: the silent driver calls the music's callback every time
      if (!polling_ && (musicSeg_ || musicOff_)) {
        polling_ = true;
        cpu.callFar(musicSeg_, musicOff_);
        polling_ = false;
      }
      break;
    case 0x0b:  // the frame's callback
      frameCallback_ = {0, cpu.lo(Cpu::BX), cpu.s[Cpu::ES], cpu.r[Cpu::DX]};
      haveFrameCallback_ = true;
      break;
    case 0x0c: {  // a callback at a scanline; one already at that line is taken out instead
      const u16 line = cpu.r[Cpu::CX];
      auto it = std::find_if(lineCallbacks_.begin(), lineCallbacks_.end(), [&](const Callback& c) { return c.scanline == line; });
      if (it != lineCallbacks_.end()) {
        lineCallbacks_.erase(it);
      } else {
        lineCallbacks_.push_back({line, cpu.lo(Cpu::BX), cpu.s[Cpu::ES], cpu.r[Cpu::DX]});
        std::stable_sort(lineCallbacks_.begin(), lineCallbacks_.end(), [](const Callback& a, const Callback& b) { return a.scanline < b.scanline; });
      }
      break;
    }
    case 0x0d:  // the timer goes back to the system
      haveFrameCallback_ = false;
      lineCallbacks_.clear();
      break;
    case 0x13:
      musicSeg_ = cpu.s[Cpu::ES];
      musicOff_ = cpu.r[Cpu::DX];
      break;
    case 0x14: {  // the callback is called with BL in AL, and what it leaves in AL is returned
      u8 result = cpu.lo(Cpu::BX);
      if (musicSeg_ || musicOff_) {
        cpu.setLo(Cpu::AX, result);
        result = static_cast<u8>(cpu.callFar(musicSeg_, musicOff_));
      }
      driverStatus_ = result;
      break;
    }
    case 0x15: driverStatus_ = 0x0a; break;
    case 0x16:  // frames counted since function 0x17
      cpu.r[Cpu::AX] = static_cast<u16>(ticks_);
      cpu.r[Cpu::DX] = static_cast<u16>(ticks_ >> 16);
      plainReturn = false;
      break;
    case 0x17: ticks_ = 0; break;
    case 0x18: callbacksOn_ = cpu.lo(Cpu::BX) != 0; break;
    case 0x09: plainReturn = false; break;
    // These do nothing in the silent driver: volume, music position, effects, loading.
    case 0x01: case 0x02: case 0x03: case 0x06: case 0x07: case 0x0a: case 0x0e: case 0x0f: case 0x10: case 0x11: case 0x12:
      break;
    default:
      break;
  }
  if (plainReturn) {
    cpu.setLo(Cpu::AX, driverStatus_ == 0xff ? 0 : driverStatus_);
    driverStatus_ = 0xff;
  }
}

void Machine::setLoop(u16 codeSegment, u16 offset) {
  loopAt_ = (u32{seg(codeSegment)} << 16) | offset;
  cpu.watch[loopAt_] = [this] {
    if (++loops_ >= loopLimit_) cpu.pause = true;
  };
}

void Machine::boot() {
  if (!loopAt_) throw CpuError("boot() needs setLoop()");
  loops_ = 0;
  loopLimit_ = 1;
  cpu.pause = false;
  for (std::uint64_t i = 0; i < 400'000'000 && !cpu.pause && !exited_; ++i) cpu.step();
  if (!cpu.pause) throw CpuError("the program never reached its loop");
  ticks_ = 0;
}

void Machine::frame() {
  // Not in the middle of something the program does with interrupts off.
  for (int i = 0; i < 100000 && !cpu.flag(Cpu::IF) && !exited_; ++i) cpu.step();
  for (u8 code : keys_) {
    keyPort_ = code;
    const u16 off = peek16(0, 9 * 4), seg = peek16(0, 9 * 4 + 2);
    if (seg || off) cpu.callInterrupt(seg, off);
  }
  keys_.clear();
  vga.startFrame();
  if (callbacksOn_ && haveFrameCallback_) {
    ++ticks_;
    callBack(frameCallback_);
    // the list may change under a callback
    for (std::size_t i = 0; i < lineCallbacks_.size(); ++i) callBack(lineCallbacks_[i]);
  }
  if (loopAt_) {
    // The loop is where boot() or the last frame left it: at its first instruction.
    loops_ = -1;
    loopLimit_ = loopsPerFrame;
    cpu.pause = false;
    for (std::uint64_t i = 0; i < 2'000'000 && !cpu.pause && !exited_; ++i) cpu.step();
    loopLimit_ = 1 << 30;  // the callbacks and keys of the next frame are not counted
  } else if (!exited_) {
    cpu.run(loopBudget);
  }
  ++frames;
}

}  // namespace oracle
