#include "core/debug/Assert.h"

#include "core/logging/Log.h"

#include <cstdlib>

namespace olam::detail
{

    void assertionFailed(const char *expression, const char *file, int line)
    {
        logging::error(LogCategory::Core, "Assertion failed: {} ({}:{})", expression, file, line);
        std::abort();
    }

} // namespace olam::detail
