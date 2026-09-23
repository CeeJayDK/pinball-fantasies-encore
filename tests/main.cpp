#include "Test.h"

#include <cstdio>
#include <vector>

namespace test {
std::vector<Case>& registry() { static std::vector<Case> r; return r; }
int& failures() { static int f = 0; return f; }
}  // namespace test

int main() {
  int failedCases = 0;
  for (const auto& c : test::registry()) {
    const int before = test::failures();
    std::printf("[ RUN  ] %s\n", c.name);
    c.fn();
    if (test::failures() != before) ++failedCases;
    std::printf("[ %s ] %s\n", test::failures() != before ? "FAIL" : " OK ", c.name);
  }
  std::printf("%zu cases, %d failed\n", test::registry().size(), failedCases);
  return failedCases ? 1 : 0;
}
