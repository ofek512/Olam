#pragma once

namespace olam::detail
{

    [[noreturn]] void assertionFailed(const char *expression, const char *file, int line);

} // namespace olam::detail

// Programmer-error checks; compiled out in release builds (NDEBUG).
#ifdef NDEBUG
#define OLAM_ASSERT(condition)   \
    do                           \
    {                            \
        (void)sizeof(condition); \
    } while (false)
#else
#define OLAM_ASSERT(condition)                                               \
    do                                                                       \
    {                                                                        \
        if (!(condition))                                                    \
            ::olam::detail::assertionFailed(#condition, __FILE__, __LINE__); \
    } while (false)
#endif
