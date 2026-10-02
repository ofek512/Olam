#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace olam
{

    class WorldGenContext;

    // One stage of the generation pipeline. Reads existing world data, writes its own outputs.
    class WorldGenerationPass
    {
    public:
        virtual ~WorldGenerationPass() = default;

        virtual std::string_view name() const = 0;

        // Fixed id used to derive this pass's seed from the world seed (see worldgen/SeedIds.h).
        virtual std::uint64_t seedId() const = 0;

        // Returns an error if required inputs (earlier passes' layers) are missing.
        virtual std::optional<std::string> validatePreconditions(const WorldGenContext &) const { return std::nullopt; }

        virtual void run(WorldGenContext &context) = 0;
    };

} // namespace olam
