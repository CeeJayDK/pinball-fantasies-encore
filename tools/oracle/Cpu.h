#pragma once
// A processor for the referee (docs/own-engine.md): the 16-bit real-mode instructions the
// game's programs use, which are the 8086's and the 80186's. It knows nothing of the machine
// around it: memory, ports and interrupts are the Bus's.
#include <cstdint>
#include <functional>
#include <map>
#include <vector>
#include <stdexcept>
#include <string>

namespace oracle {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;

struct CpuError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class Bus {
 public:
  virtual ~Bus() = default;
  virtual u8 read(u32 address) = 0;
  virtual void write(u32 address, u8 value) = 0;
  virtual u8 portIn(u16 port) = 0;
  virtual void portOut(u16 port, u8 value) = 0;
  /// A software interrupt. True if the machine answered it itself; false to go through the
  /// vector in memory, as the processor would.
  virtual bool interrupt(u8 number) = 0;
};

class Cpu {
 public:
  enum Reg { AX, CX, DX, BX, SP, BP, SI, DI };
  enum Seg { ES, CS, SS, DS };
  enum Flag : u16 { CF = 1, PF = 4, AF = 0x10, ZF = 0x40, SF = 0x80, TF = 0x100, IF = 0x200, DF = 0x400, OF = 0x800 };

  explicit Cpu(Bus& bus) : bus_(bus) {}

  u16 r[8]{};
  u16 s[4]{};
  u16 ip = 0;
  u16 flags = 2;
  bool halted = false;
  /// Called when the instruction at a place (segment << 16 | offset) is about to run.
  std::map<u32, std::function<void()>> watch;
  /// Set by a watch to leave that instruction for later: step() returns without running it.
  bool pause = false;
  /// If set: every offset in segment `coverageSegment` an instruction began at is marked.
  std::vector<bool>* coverage = nullptr;
  u16 coverageSegment = 0;
  std::uint64_t executed = 0;

  u8 lo(Reg x) const { return static_cast<u8>(r[x]); }
  u8 hi(Reg x) const { return static_cast<u8>(r[x] >> 8); }
  void setLo(Reg x, u8 v) { r[x] = static_cast<u16>((r[x] & 0xff00) | v); }
  void setHi(Reg x, u8 v) { r[x] = static_cast<u16>((r[x] & 0x00ff) | (v << 8)); }
  bool flag(Flag f) const { return flags & f; }
  void setFlag(Flag f, bool on) { flags = static_cast<u16>(on ? flags | f : flags & ~f); }

  u8 read8(u16 seg, u16 off) { return bus_.read((u32{seg} << 4) + off); }
  u16 read16(u16 seg, u16 off) { return static_cast<u16>(read8(seg, off) | (read8(seg, static_cast<u16>(off + 1)) << 8)); }
  void write8(u16 seg, u16 off, u8 v) { bus_.write((u32{seg} << 4) + off, v); }
  void write16(u16 seg, u16 off, u16 v) {
    write8(seg, off, static_cast<u8>(v));
    write8(seg, static_cast<u16>(off + 1), static_cast<u8>(v >> 8));
  }
  void push(u16 v) { r[SP] = static_cast<u16>(r[SP] - 2); write16(s[SS], r[SP], v); }
  u16 pop() { const u16 v = read16(s[SS], r[SP]); r[SP] = static_cast<u16>(r[SP] + 2); return v; }

  /// One instruction (a repeated string instruction counts as one).
  void step();
  /// Up to `limit` instructions; stops early at a hlt.
  void run(std::uint64_t limit);
  /// Calls a far routine that ends with retf, as an interrupt handler of the machine would:
  /// every register is as it was afterwards. Throws if it has not returned after `limit`.
  /// Returns what the routine left in AX.
  u16 callFar(u16 seg, u16 off, std::uint64_t limit = 50'000'000);
  /// The same for a routine that ends with iret (an interrupt vector).
  u16 callInterrupt(u16 seg, u16 off, std::uint64_t limit = 50'000'000);
  /// The vector in memory.
  void raise(u8 number);

  std::string where() const;
  /// Where the last instructions were, oldest first, for when something goes wrong.
  std::string history() const;

 private:
  struct Operand {
    bool isReg = false;
    int reg = 0;
    u16 seg = 0, off = 0;
  };
  u8 fetch8() { return read8(s[CS], ip++); }
  u16 fetch16() { const u16 v = read16(s[CS], ip); ip = static_cast<u16>(ip + 2); return v; }
  Operand modrm(u8 m);
  u8 get8(const Operand& o);
  u16 get16(const Operand& o);
  void set8(const Operand& o, u8 v);
  void set16(const Operand& o, u16 v);
  u8 reg8(int n) const { return n < 4 ? lo(static_cast<Reg>(n)) : hi(static_cast<Reg>(n - 4)); }
  void setReg8(int n, u8 v) { n < 4 ? setLo(static_cast<Reg>(n), v) : setHi(static_cast<Reg>(n - 4), v); }
  u16 dataSeg(Seg normal) const { return segOverride_ >= 0 ? s[segOverride_] : s[normal]; }

  void setSzp8(u8 v);
  void setSzp16(u16 v);
  u8 alu8(int op, u8 a, u8 b);
  u16 alu16(int op, u16 a, u16 b);
  u8 shift8(int op, u8 v, int count);
  u16 shift16(int op, u16 v, int count);
  u8 inc8(u8 v, bool dec);
  u16 inc16(u16 v, bool dec);
  void string(u8 opcode);
  bool condition(int cc) const;
  u16 callRoutine(u16 seg, u16 off, bool withFlags, std::uint64_t limit);
  [[noreturn]] void bad(u8 opcode);

  Bus& bus_;
  int segOverride_ = -1;
  int rep_ = 0;  ///< 0 none, 1 repe/rep, 2 repne
  u16 startIp_ = 0;
  static constexpr int kHistory = 48;
  u32 history_[kHistory]{};
  int historyAt_ = 0;
};

}  // namespace oracle
