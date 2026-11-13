#include "TestFramework.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace actest
{
static int failures = 0;
static thread_local bool counting = false;
static thread_local long allocations = 0;

std::vector<TestCase>& registry()
{
    static std::vector<TestCase> r;
    return r;
}

void reportFailure (const char* file, int line, const std::string& msg)
{
    ++failures;
    std::printf ("    FAIL %s:%d  %s\n", file, line, msg.c_str());
}

void AllocationCounter::start() { allocations = 0; counting = true; }
long AllocationCounter::stop() { counting = false; return allocations; }
} // namespace actest

// Global allocation hooks so tests can prove process() never allocates.
void* operator new (std::size_t n)
{
    if (actest::counting) ++actest::allocations;
    if (void* p = std::malloc (n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n) { return operator new (n); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

int main (int argc, char** argv)
{
    const std::string filter = argc > 1 ? argv[1] : "";
    int run = 0;
    for (auto& t : actest::registry())
    {
        if (! filter.empty() && t.name.find (filter) == std::string::npos) continue;
        const int before = actest::failures;
        std::printf ("[ RUN  ] %s\n", t.name.c_str());
        t.fn();
        std::printf ("%s %s\n", actest::failures == before ? "[  OK  ]" : "[FAILED]", t.name.c_str());
        ++run;
    }
    std::printf ("\n%d test(s) run, %d check failure(s)\n", run, actest::failures);
    return actest::failures == 0 ? 0 : 1;
}
