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
        std::optional<std::string> loadPath;
        std::vector<std::string> errors;
    };

    // Supports --seed <number|text>, --size <W>x<H> and --load <file.olamworld>.
    CommandLineOptions parseCommandLine(int argc, char *argv[]);

} // namespace olam
