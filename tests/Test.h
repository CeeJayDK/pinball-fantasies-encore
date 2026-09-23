#pragma once
// Tiny self-contained test framework: TEST(name) { CHECK(cond); }
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace test {
struct Case { const char* name; std::function<void()> fn; };
std::vector<Case>& registry();
int& failures();
struct Registrar { Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };
}  // namespace test

#define TEST(name) \
  static void test_##name(); \
  static test::Registrar registrar_##name(#name, test_##name); \
  static void test_##name()

#define CHECK(cond) \
  do { if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++test::failures(); } } while (0)
#define CHECK_EQ(a, b) \
  do { if (!((a) == (b))) { std::printf("  FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); ++test::failures(); } } while (0)
