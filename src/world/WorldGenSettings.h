#pragma once

#include <optional>
#include <string>

namespace olam
{

    // Tuning values for every generation pass. Part of WorldConfig, so they are validated, hashed and saved.
    // Each pass adds its own group when it is implemented.
    struct WorldGenSettings
    {
    };

    // Calls visit(field) for every setting in a fixed order (hashing, saving, loading).
    template <typename Settings, typename Visitor>
    void visitGenerationSettings(Settings &settings, Visitor &&visit)
    {
        (void)settings;
        (void)visit;
    }

    std::optional<std::string> validateGenerationSettings(const WorldGenSettings &settings);

} // namespace olam
