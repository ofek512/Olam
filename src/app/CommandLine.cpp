#include "app/CommandLine.h"

#include "core/random/Seed.h"

#include <charconv>
#include <string_view>

namespace olam
{

    namespace
    {

        std::optional<int> parseInt(std::string_view text)
        {
            int value = 0;
            const char *end = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(text.data(), end, value);
            if (ec != std::errc{} || ptr != end)
                return std::nullopt;
            return value;
        }

    } // namespace

    CommandLineOptions parseCommandLine(int argc, char *argv[])
    {
        CommandLineOptions options;

        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg = argv[i];
            const bool hasValue = i + 1 < argc;

            if (arg == "--seed")
            {
                if (!hasValue)
                {
                    options.errors.push_back("--seed requires a value");
                    continue;
                }
                options.seed = parseSeed(argv[++i]);
            }
            else if (arg == "--size")
            {
                if (!hasValue)
                {
                    options.errors.push_back("--size requires a value like 2048x1024");
                    continue;
                }
                const std::string_view value = argv[++i];
                const std::size_t separator = value.find_first_of("xX");
                const auto width = separator == std::string_view::npos ? std::nullopt : parseInt(value.substr(0, separator));
                const auto height = separator == std::string_view::npos ? std::nullopt : parseInt(value.substr(separator + 1));
                if (!width || !height)
                {
                    options.errors.push_back("invalid --size '" + std::string(value) + "', expected <W>x<H>");
                    continue;
                }
                options.worldWidth = width;
                options.worldHeight = height;
            }
            else if (arg == "--load")
            {
                if (!hasValue)
                {
                    options.errors.push_back("--load requires a file path");
                    continue;
                }
                options.loadPath = argv[++i];
            }
            else
            {
                options.errors.push_back("unknown argument '" + std::string(arg) + "'");
            }
        }

        return options;
    }

} // namespace olam
