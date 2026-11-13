// Minimal self-contained test harness (no external dependencies).
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace actest
{
struct TestCase { std::string name; std::function<void()> fn; };
std::vector<TestCase>& registry();
void reportFailure (const char* file, int line, const std::string& msg);

struct Registrar
{
    Registrar (const char* name, std::function<void()> fn) { registry().push_back ({ name, std::move (fn) }); }
};

/** Counts heap allocations made by the current thread while enabled. */
struct AllocationCounter
{
    static void start();
    static long stop();
};
} // namespace actest

#define AC_CAT2(a, b) a##b
#define AC_CAT(a, b) AC_CAT2 (a, b)
#define TEST_CASE(name)                                                                     \
    static void AC_CAT (test_fn_, __LINE__)();                                              \
    static actest::Registrar AC_CAT (test_reg_, __LINE__) (name, AC_CAT (test_fn_, __LINE__)); \
    static void AC_CAT (test_fn_, __LINE__)()

#define CHECK(cond)                                                                         \
    do { if (! (cond)) actest::reportFailure (__FILE__, __LINE__, "CHECK(" #cond ")"); } while (0)

#define CHECK_NEAR(a, b, tol)                                                               \
    do {                                                                                    \
        const double va_ = (double) (a), vb_ = (double) (b);                                \
        if (! (std::abs (va_ - vb_) <= (double) (tol)))                                     \
        {                                                                                   \
            char buf_[256];                                                                 \
            std::snprintf (buf_, sizeof (buf_), "CHECK_NEAR(%s, %s, %s): %.6g vs %.6g",     \
                           #a, #b, #tol, va_, vb_);                                         \
            actest::reportFailure (__FILE__, __LINE__, buf_);                               \
        }                                                                                   \
    } while (0)

#define INFO(...) do { std::printf ("    · "); std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)
