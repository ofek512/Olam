#pragma once

#include "core/serialization/ByteReader.h"
#include "core/serialization/ByteWriter.h"
#include "world/World.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace olam
{

    // .olamworld: "OLAMWRLD", version, config, seed, present layers (LayerId, size, values), hydrology and
    // resource entities, then the world hash, verified on load. Little-endian, uncompressed.
    inline constexpr std::uint32_t kWorldFileVersion = 1;

    std::vector<std::uint8_t> serializeWorld(const World &world);

    struct WorldLoadResult
    {
        std::unique_ptr<World> world;
        // Set when loading failed; world is then null.
        std::string error;
    };

    // Validates everything read; corrupt or tampered data yields an error, never an invalid World.
    WorldLoadResult deserializeWorld(std::span<const std::uint8_t> bytes);

    // Returns an error description, or nullopt on success.
    std::optional<std::string> saveWorld(const World &world, const std::filesystem::path &path);
    WorldLoadResult loadWorld(const std::filesystem::path &path);

    // Entity sections, shared by the file format and the world hash.
    void writeHydrologyEntities(ByteWriter &writer, const HydrologyData &hydrology);
    void writeResourceEntities(ByteWriter &writer, const ResourceData &resources);

} // namespace olam
