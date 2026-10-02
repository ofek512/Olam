#include "TestFramework.h"

int main()
{
    int failedTests = 0;
    for (const auto &test : olam::test::registry())
    {
        const int before = olam::test::failureCount();
        test.body();
        const bool passed = olam::test::failureCount() == before;
        if (!passed)
            ++failedTests;
        std::printf("[%s] %s\n", passed ? " OK " : "FAIL", test.name);
    }

    std::printf("\n%zu tests, %d failed\n", olam::test::registry().size(), failedTests);
    return failedTests == 0 ? 0 : 1;
}
