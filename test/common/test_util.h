#pragma once

#include <cstdio>
#include <sstream>
#include <string>

namespace test {

inline int g_checks = 0;
inline int g_failures = 0;

inline void section(const char *name)
{
    std::printf("\n[----------] %s\n", name);
}

inline void pass(const char *file, int line, const char *expr)
{
    ++g_checks;
    std::printf("[       OK ] %s:%d: %s\n", file, line, expr);
}

inline void fail(const char *file, int line, const char *expr,
                 const std::string &detail = std::string())
{
    ++g_checks;
    ++g_failures;
    std::printf("[  FAILED  ] %s:%d: %s", file, line, expr);
    if (!detail.empty())
        std::printf("  (%s)", detail.c_str());
    std::printf("\n");
}

template <typename T>
std::string str(const T &value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

inline std::string str(bool value) { return value ? "true" : "false"; }
inline std::string str(const std::string &value) { return value; }
inline std::string str(const char *value) { return value ? value : "(null)"; }

inline int summary()
{
    if (g_failures == 0) {
        std::printf("\n[  PASSED  ] %d check(s)\n", g_checks);
        return 0;
    }
    std::printf("\n[  FAILED  ] %d of %d check(s) failed\n", g_failures, g_checks);
    return 1;
}

}

#define TEST_SECTION(name) ::test::section(name)

#define CHECK(cond)                                                  \
    do {                                                             \
        if (cond)                                                    \
            ::test::pass(__FILE__, __LINE__, #cond);                 \
        else                                                         \
            ::test::fail(__FILE__, __LINE__, #cond);                 \
    } while (0)

#define CHECK_EQ(actual, expected)                                          \
    do {                                                                    \
        const auto _actual = (actual);                                      \
        const auto _expected = (expected);                                  \
        if (_actual == _expected)                                           \
            ::test::pass(__FILE__, __LINE__, #actual " == " #expected);     \
        else                                                                \
            ::test::fail(__FILE__, __LINE__, #actual " == " #expected,      \
                         ::test::str(_actual) + " != " + ::test::str(_expected)); \
    } while (0)

#define CHECK_THROWS(expr, extype)                                      \
    do {                                                                \
        bool _thrown = false;                                           \
        try {                                                           \
            (void)(expr);                                               \
        } catch (const extype &) {                                      \
            _thrown = true;                                             \
        } catch (...) {                                                 \
        }                                                               \
        if (_thrown)                                                    \
            ::test::pass(__FILE__, __LINE__, "throws " #extype ": " #expr); \
        else                                                            \
            ::test::fail(__FILE__, __LINE__, "throws " #extype ": " #expr); \
    } while (0)
