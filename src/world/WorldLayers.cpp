#include "world/WorldLayers.h"

#include "world/queries/HydrologyQueries.h"

#include <array>
#include <format>

namespace olam
{

    namespace
    {

        constexpr WorldLayerDescriptor kDescriptors[] = {
            {LayerId::PlateId, "Plate", ""},
            {LayerId::RockType, "Rock", ""},
            {LayerId::Elevation, "Elevation", "m"},
            {LayerId::SurfaceWater, "Water", ""},
            {LayerId::DistanceToOcean, "To ocean", "km"},
            {LayerId::Temperature, "Temperature", "C"},
            {LayerId::Rainfall, "Rainfall", "mm/yr"},
            {LayerId::Moisture, "Moisture", ""},
            {LayerId::FlowDirection, "Flow", ""},
            {LayerId::Discharge, "Discharge", "m3/s"},
            {LayerId::RiverId, "River", ""},
            {LayerId::LakeId, "Lake", ""},
            {LayerId::Soil, "Soil", ""},
            {LayerId::Biome, "Biome", ""},
            {LayerId::Fertility, "Fertility", "%"},
            {LayerId::Vegetation, "Vegetation", ""},
            {LayerId::TreeCover, "Tree cover", "%"},
            {LayerId::DepositId, "Deposit", ""},
        };

        constexpr std::string_view kDirectionNames[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

    } // namespace

    std::span<const WorldLayerDescriptor> worldLayerDescriptors()
    {
        return kDescriptors;
    }

    bool isLayerPresent(const World &world, LayerId id)
    {
        bool present = false;
        visitLayer(world, id, [&](const auto &layer)
                   { present = !layer.empty(); });
        return present;
    }

    std::string formatLayerValue(const World &world, LayerId id, std::size_t index)
    {
        switch (id)
        {
        case LayerId::PlateId:
            return std::format("{}", world.terrain().plateId[index]);
        case LayerId::RockType:
            return std::string(toString(world.terrain().rockType[index]));
        case LayerId::Elevation:
            return std::format("{} m", world.terrain().elevation[index]);
        case LayerId::SurfaceWater:
            return std::string(toString(world.hydrology().surfaceWater[index]));
        case LayerId::DistanceToOcean:
            return std::format("{} km", world.hydrology().distanceToOceanKm[index]);
        case LayerId::Temperature:
            return std::format("{:.1f} C", world.climate().meanAnnualTemperature[index] / 10.0);
        case LayerId::Rainfall:
            return std::format("{} mm/yr", world.climate().annualRainfall[index]);
        case LayerId::Moisture:
        {
            const double aridity = world.climate().moisture[index] / 255.0 * 2.0;
            const char *label = aridity < 0.05   ? "hyper-arid"
                                : aridity < 0.2  ? "arid"
                                : aridity < 0.5  ? "semi-arid"
                                : aridity < 0.65 ? "dry sub-humid"
                                                 : "humid";
            return std::format("AI {:.2f} ({})", aridity, label);
        }
        case LayerId::FlowDirection:
        {
            const auto direction = static_cast<std::size_t>(world.hydrology().flowDirection[index]);
            return direction < kDirection8Count ? std::string(kDirectionNames[direction]) : std::string("-");
        }
        case LayerId::Discharge:
            return std::format("{:.2f} m3/s", world.hydrology().discharge[index] / 100.0);
        case LayerId::RiverId:
        {
            const RiverId riverId = world.hydrology().riverId[index];
            if (!riverId.isValid())
                return "-";
            const River &river = world.hydrology().rivers[riverId.index()];
            return std::format("#{} {}, {} tiles -> {}", riverId.value,
                               toString(riverClassForDischarge(world.config().generation.hydrology, river.mouthDischarge)),
                               river.path.size(), toString(river.endsIn));
        }
        case LayerId::LakeId:
        {
            const LakeId lakeId = world.hydrology().lakeId[index];
            if (!lakeId.isValid())
                return "-";
            const Lake &lake = world.hydrology().lakes[lakeId.index()];
            return std::format("#{} surface {} m, {} tiles", lakeId.value, lake.surfaceElevation, lake.tileCount);
        }
        case LayerId::Soil:
            return std::string(toString(world.geography().soil[index]));
        case LayerId::Biome:
            return std::string(toString(world.geography().biome[index]));
        case LayerId::Fertility:
            return std::format("{:.0f} %", world.geography().fertility[index] / 2.55);
        case LayerId::Vegetation:
            return std::string(toString(world.geography().vegetation[index]));
        case LayerId::TreeCover:
            return std::format("{} %", world.geography().treeCover[index]);
        case LayerId::DepositId:
        {
            const DepositId depositId = world.resources().depositId[index];
            if (!depositId.isValid())
                return "-";
            const Deposit &deposit = world.resources().deposits[depositId.index()];
            return std::format("#{} {} (richness {} %, {} tiles)", depositId.value, toString(deposit.mineral),
                               deposit.richness, deposit.tileCount);
        }
        case LayerId::Count:
            break;
        }
        return {};
    }

} // namespace olam
