// Shared helpers for the host-side test binaries. Each test .cpp is compiled
// into its own executable, so the inline variable below is per-binary state.
#ifndef BTHOME_TESTS_TEST_UTILS_H
#define BTHOME_TESTS_TEST_UTILS_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

inline int g_failures = 0;

inline void expect_true(const char *name, bool cond)
{
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond)
    {
        ++g_failures;
    }
}

inline void expect_bytes(const char *name,
                         const std::uint8_t *got,
                         std::size_t got_len,
                         const std::uint8_t *want,
                         std::size_t want_len)
{
    const bool ok = (got_len == want_len) && (std::memcmp(got, want, want_len) == 0);
    std::printf("[%s] %s\n  got : ", ok ? "PASS" : "FAIL", name);
    for (std::size_t i = 0; i < got_len; ++i)
    {
        std::printf("%02X ", got[i]);
    }
    std::printf("\n  want: ");
    for (std::size_t i = 0; i < want_len; ++i)
    {
        std::printf("%02X ", want[i]);
    }
    std::printf("\n");

    if (!ok)
    {
        ++g_failures;
    }
}

// Prints the final verdict and returns the process exit code for main().
inline int test_summary()
{
    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}

#endif // BTHOME_TESTS_TEST_UTILS_H
