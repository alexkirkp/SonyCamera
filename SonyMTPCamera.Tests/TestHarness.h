#pragma once
// A deliberately tiny test harness: no framework to install, so the tests build the same way on a
// developer machine and on the CI runner and exit non-zero when anything fails.
#include <cstdio>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace testing
{
    struct TestCase { const char* name; std::function<void()> body; };

    inline std::vector<TestCase>& Registry() { static std::vector<TestCase> r; return r; }
    inline int& Failures() { static int f = 0; return f; }

    struct Registrar
    {
        Registrar(const char* name, std::function<void()> body) { Registry().push_back({ name, std::move(body) }); }
    };

    inline void Fail(const char* file, int line, const std::string& message)
    {
        std::printf("    FAILED %s:%d: %s\n", file, line, message.c_str());
        Failures()++;
    }
}

#define TEST(name) \
    static void name(); \
    static testing::Registrar registrar_##name(#name, name); \
    static void name()

#define CHECK(cond) \
    do { if (!(cond)) testing::Fail(__FILE__, __LINE__, #cond); } while (0)

#define CHECK_EQ(actual, expected) \
    do { auto a_ = (actual); auto e_ = (expected); \
         if (!(a_ == e_)) testing::Fail(__FILE__, __LINE__, std::string(#actual " != " #expected)); } while (0)
