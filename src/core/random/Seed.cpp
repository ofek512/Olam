#include "core/random/Seed.h"

#include "core/hash/XxHash64.h"

#include <charconv>
#include <system_error>

namespace olam
{

    std::uint64_t parseSeed(std::string_view text)
    {
        if (!text.empty() && text.front() >= '0' && text.front() <= '9')
        {
            std::uint64_t value = 0;
            const char *end = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(text.data(), end, value);
            if (ec == std::errc{} && ptr == end)
                return value;
        }
        return xxHash64(text);
    }

} // namespace olam
