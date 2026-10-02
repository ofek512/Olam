#pragma once

// Minimal test harness; avoids adding an external test dependency.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace olam::test
{

    struct TestCase
    {
        const char *name;
        std::function<void()> body;
    };

    inline std::vector<TestCase> &registry()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    inline int &failureCount()
    {
        static int failures = 0;
        return failures;
    }

    struct Registrar
    {
        Registrar(const char *name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
    };

    inline void reportFailure(const char *file, int line, const std::string &message)
    {
        ++failureCount();
        std::fprintf(stderr, "  FAILED %s(%d): %s\n", file, line, message.c_str());
    }

} // namespace olam::test

#define OLAM_TEST_CONCAT_IMPL(a, b) a##b
#define OLAM_TEST_CONCAT(a, b) OLAM_TEST_CONCAT_IMPL(a, b)

#define OLAM_TEST(name)                                                             \
    static void name();                                                             \
    static ::olam::test::Registrar OLAM_TEST_CONCAT(name, _registrar)(#name, name); \
    static void name()

#define OLAM_CHECK(condition)                                            \
    do                                                                   \
    {                                                                    \
        if (!(condition))                                                \
            ::olam::test::reportFailure(__FILE__, __LINE__, #condition); \
    } while (false)

#define OLAM_CHECK_NEAR(actual, expected, epsilon)                                       \
    do                                                                                   \
    {                                                                                    \
        const double olamActual = static_cast<double>(actual);                           \
        const double olamExpected = static_cast<double>(expected);                       \
        if (std::fabs(olamActual - olamExpected) > static_cast<double>(epsilon))         \
            ::olam::test::reportFailure(__FILE__, __LINE__,                              \
                                        std::string(#actual " ~= " #expected " (got ") + \
                                            std::to_string(olamActual) + ", expected " + \
                                            std::to_string(olamExpected) + ")");         \
    } while (false)
