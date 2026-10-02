#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace olam
{

    struct CommandLineOptions
    {
        std::optional<std::uint64_t> seed;
        std::optional<int> worldWidth;
        std::optional<int> worldHeight;
        std::vector<std::string> errors;
    };

    // Supports --seed <number|text> and --size <W>x<H>.
    CommandLineOptions parseCommandLine(int argc, char *argv[]);

} // namespace olam
