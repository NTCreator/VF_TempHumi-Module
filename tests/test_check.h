/* Minimal check macros for the host tests: count, print the failing line,
   and let main() return the failure count as the exit code. */
#ifndef TEST_CHECK_H
#define TEST_CHECK_H

#include <stdio.h>

static int g_checks;
static int g_failures;

#define CHECK(cond) \
  do { \
    g_checks++; \
    if (!(cond)) { \
      g_failures++; \
      printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    } \
  } while (0)

#define CHECK_EQ(a, b) \
  do { \
    const long long va_ = (long long)(a); \
    const long long vb_ = (long long)(b); \
    g_checks++; \
    if (va_ != vb_) { \
      g_failures++; \
      printf("  FAIL %s:%d  %s == %s  (%lld != %lld)\n", __FILE__, __LINE__, #a, #b, va_, vb_); \
    } \
  } while (0)

#define TEST_REPORT(name) \
  (printf("%s: %d checks, %d failures\n", (name), g_checks, g_failures), g_failures)

#endif /* TEST_CHECK_H */
