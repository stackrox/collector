#include "AbortHandler.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

void test_crash() {
  raise(SIGSEGV);
}

TEST(AbortHandler, Crash) {
#if defined(__powerpc64__)
  GTEST_SKIP() << "Skipping AbortHandler test on ppc64le";
#endif

  // Install the AbortHandler, and verify that the stderr output will contain
  // something that looks like a stacktrace with AbortHander & Crash_Test frames.
  signal(SIGSEGV, AbortHandler);
  ASSERT_EXIT(test_crash(), ::testing::KilledBySignal(SIGSEGV),
              ".*AbortHandler.*AbortHandler_Crash_Test.*");
}
