#pragma once

#include "world/TileView.h"
#include "world/WorldConfig.h"
#include "world/WorldCoord.h"
#include "world/WorldData.h"

#include <cstddef>
#include <cstdint>

namespace olam
{

    // The strategic world: configuration, seed and persistent data. Owns data only.
    class World
    {
    public:
        // config must be valid (see validateWorldConfig).
        World(const WorldConfig &config, std::uint64_t seed);

        const WorldConfig &config() const { return m_config; }
        std::uint64_t seed() const { return m_seed; }

        int width() const { return m_config.width; }
        int height() const { return m_config.height; }
        std::size_t tileCount() const { return static_cast<std::size_t>(width()) * static_cast<std::size_t>(height()); }

        bool isValid(WorldCoord coord) const
        {
            return coord.x >= 0 && coord.y >= 0 && coord.x < width() && coord.y < height();
        }

        std::size_t index(WorldCoord coord) const;
        WorldCoord coordFromIndex(std::size_t index) const;

        // Latitude in degrees at the centre of tile row y; north positive.
        double latitudeAt(int y) const;

        TileView tile(WorldCoord coord) const { return TileView(*this, coord); }

        TerrainData &terrain() { return m_terrain; }
        const TerrainData &terrain() const { return m_terrain; }
        ClimateData &climate() { return m_climate; }
        const ClimateData &climate() const { return m_climate; }
        HydrologyData &hydrology() { return m_hydrology; }
        const HydrologyData &hydrology() const { return m_hydrology; }
        GeographyData &geography() { return m_geography; }
        const GeographyData &geography() const { return m_geography; }

    private:
        WorldConfig m_config;
        std::uint64_t m_seed;
        TerrainData m_terrain;
        ClimateData m_climate;
        HydrologyData m_hydrology;
        GeographyData m_geography;
    };

} // namespace olam
