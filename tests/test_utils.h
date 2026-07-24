// Shared helpers for the host-side test binaries. Each test .cpp is compiled
// into its own executable, so the inline variable below is per-binary state.
#ifndef BTHOME_TESTS_TEST_UTILS_H
#define BTHOME_TESTS_TEST_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

inline int g_failures = 0;

inline void expect_true(const char *name, bool cond)
{
    printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond)
    {
        ++g_failures;
    }
}

inline void expect_bytes(const char *name,
                         const uint8_t *got,
                         size_t got_len,
                         const uint8_t *want,
                         size_t want_len)
{
    const bool ok = (got_len == want_len) && (memcmp(got, want, want_len) == 0);
    printf("[%s] %s\n  got : ", ok ? "PASS" : "FAIL", name);
    for (size_t i = 0; i < got_len; ++i)
    {
        printf("%02X ", got[i]);
    }
    printf("\n  want: ");
    for (size_t i = 0; i < want_len; ++i)
    {
        printf("%02X ", want[i]);
    }
    printf("\n");

    if (!ok)
    {
        ++g_failures;
    }
}

// Prints the final verdict and returns the process exit code for main().
inline int test_summary()
{
    printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}

#endif // BTHOME_TESTS_TEST_UTILS_H
