#include "worldgen/passes/HydrologyPass.h"

#include "core/math/MathUtil.h"
#include "core/noise/Noise.h"
#include "world/World.h"
#include "worldgen/WorkingLayers.h"
#include "worldgen/WorldGenContext.h"
#include "worldgen/passes/RainfallPass.h"
#include "worldgen/util/DistanceTransform.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace olam
{

    namespace
    {

        constexpr std::uint32_t kNone = std::numeric_limits<std::uint32_t>::max();
        constexpr double kSecondsPerYear = 365.25 * 24.0 * 3600.0;

        struct FloodEntry
        {
            std::int32_t height;
            // Insertion order: equal heights are processed first-in first-out, so flats drain like a BFS.
            std::uint32_t order;
            std::uint32_t index;
        };

        struct FloodsLater
        {
            bool operator()(const FloodEntry &a, const FloodEntry &b) const
            {
                if (a.height != b.height)
                    return a.height > b.height;
                return a.order > b.order;
            }
        };

        Direction8 opposite(Direction8 direction)
        {
            return static_cast<Direction8>((static_cast<std::size_t>(direction) + 4) % kDirection8Count);
        }

        // Orthogonal directions first so coastal tiles prefer straight mouths.
        constexpr Direction8 kOutletSearchOrder[] = {
            Direction8::North,
            Direction8::East,
            Direction8::South,
            Direction8::West,
            Direction8::NorthEast,
            Direction8::SouthEast,
            Direction8::SouthWest,
            Direction8::NorthWest,
        };

        // Where an outlet tile drains: an adjacent ocean tile, else off the nearest map edge; kNoFlow otherwise.
        Direction8 outletDirection(const World &world, WorldCoord coord)
        {
            const auto &water = world.hydrology().surfaceWater;
            for (const Direction8 direction : kOutletSearchOrder)
            {
                const WorldCoord n = neighbor(coord, direction);
                if (world.isValid(n) && water[world.index(n)] == SurfaceWater::Ocean)
                    return direction;
            }
            if (coord.y == 0)
                return Direction8::North;
            if (coord.x == world.width() - 1)
                return Direction8::East;
            if (coord.y == world.height() - 1)
                return Direction8::South;
            if (coord.x == 0)
                return Direction8::West;
            return kNoFlow;
        }

    } // namespace

    float runoffMm(float rainfallMm, float potentialEvaporationMm)
    {
        if (rainfallMm <= 0.0f)
            return 0.0f;
        const float ratio = rainfallMm / std::max(potentialEvaporationMm, 1.0f);
        const float evapotranspiration = rainfallMm / std::sqrt(1.0f + ratio * ratio);
        return std::max(0.0f, rainfallMm - evapotranspiration);
    }

    std::optional<std::string> HydrologyPass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().climate().annualRainfall.empty())
            return std::string("requires RainfallPass");
        return std::nullopt;
    }

    void HydrologyPass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const HydrologySettings &settings = world.config().generation.hydrology;
        const int width = world.width();
        const int height = world.height();
        const std::size_t tileCount = world.tileCount();
        const auto &elevation = world.terrain().elevation;
        const auto &climate = world.climate();
        HydrologyData &hydrology = world.hydrology();
        auto &water = hydrology.surfaceWater;

        hydrology.flowDirection.resize(width, height, kNoFlow);
        hydrology.discharge.resize(width, height, 0);
        hydrology.riverId.resize(width, height, RiverId{});
        hydrology.lakeId.resize(width, height, LakeId{});
        hydrology.rivers.clear();
        hydrology.lakes.clear();
        auto &flow = hydrology.flowDirection;

        const auto downstream = [&](std::size_t index) -> std::size_t
        {
            const Direction8 direction = flow[index];
            if (direction == kNoFlow)
                return kNone;
            const WorldCoord next = neighbor(world.coordFromIndex(index), direction);
            return world.isValid(next) ? world.index(next) : kNone;
        };

        // Outlets: land next to the ocean or on the map edge.
        std::vector<std::uint32_t> outlets;
        std::size_t landTiles = 0;
        for (std::size_t i = 0; i < tileCount; ++i)
        {
            if (water[i] != SurfaceWater::Land)
                continue;
            ++landTiles;
            const Direction8 direction = outletDirection(world, world.coordFromIndex(i));
            if (direction == kNoFlow)
                continue;
            flow[i] = direction;
            outlets.push_back(static_cast<std::uint32_t>(i));
        }

        // Priority-flood over `surface` from the outlets. Each land tile drains to the tile it was reached from
        // (written to flow); filled receives the depression-filled surface; returns land tiles in flood order.
        std::vector<std::uint8_t> queued(tileCount, 0);
        const auto flood = [&](const std::vector<std::int32_t> &surface, std::vector<std::int32_t> &filled)
        {
            std::fill(queued.begin(), queued.end(), std::uint8_t{0});
            filled.assign(tileCount, 0);
            std::vector<std::uint32_t> order;
            order.reserve(landTiles);
            std::priority_queue<FloodEntry, std::vector<FloodEntry>, FloodsLater> queue;
            std::uint32_t pushes = 0;
            for (const std::uint32_t outlet : outlets)
            {
                filled[outlet] = surface[outlet];
                queued[outlet] = 1;
                queue.push({filled[outlet], pushes++, outlet});
            }
            while (!queue.empty())
            {
                const FloodEntry entry = queue.top();
                queue.pop();
                order.push_back(entry.index);
                const WorldCoord coord = world.coordFromIndex(entry.index);
                for (std::size_t d = 0; d < kDirection8Count; ++d)
                {
                    const auto direction = static_cast<Direction8>(d);
                    const WorldCoord n = neighbor(coord, direction);
                    if (!world.isValid(n))
                        continue;
                    const std::size_t ni = world.index(n);
                    if (queued[ni] || water[ni] != SurfaceWater::Land)
                        continue;
                    queued[ni] = 1;
                    filled[ni] = std::max(surface[ni], entry.height);
                    flow[ni] = opposite(direction);
                    queue.push({filled[ni], pushes++, static_cast<std::uint32_t>(ni)});
                }
            }
            // Every land component touches the ocean or the map edge, so the flood reaches all land.
            OLAM_ASSERT(order.size() == landTiles);
            return order;
        };

        // 1. Depressions from the true elevation.
        std::vector<std::int32_t> filled;
        {
            std::vector<std::int32_t> surface(tileCount, 0);
            for (std::size_t i = 0; i < tileCount; ++i)
                surface[i] = elevation[i];
            flood(surface, filled);
        }

        // Flow routing over the filled surface plus low-amplitude noise (decimetres): channels meander on smooth
        // slopes instead of following straight grid lines. Elevation itself is not changed.
        std::vector<std::uint32_t> floodOrder;
        {
            const auto tileKm = static_cast<float>(world.config().tileSizeMeters / 1000.0);
            const noise::FractalParams params{3, 1.0f / settings.routingNoiseWavelengthKm, 2.0f, 0.5f};
            const std::uint64_t seed = context.seedFor(*this);
            std::vector<std::int32_t> routing(tileCount, 0);
            for (std::size_t i = 0; i < tileCount; ++i)
            {
                if (water[i] != SurfaceWater::Land)
                    continue;
                const WorldCoord coord = world.coordFromIndex(i);
                const float n = noise::fbm(seed, (static_cast<float>(coord.x) + 0.5f) * tileKm,
                                           (static_cast<float>(coord.y) + 0.5f) * tileKm, params);
                routing[i] = filled[i] * 10 + static_cast<std::int32_t>(std::floor(n * settings.routingNoiseM * 10.0f + 0.5f));
            }
            std::vector<std::int32_t> routedFill;
            floodOrder = flood(routing, routedFill);
        }

        std::vector<std::uint32_t> floodRank(tileCount, kNone);
        for (std::size_t rank = 0; rank < floodOrder.size(); ++rank)
            floodRank[floodOrder[rank]] = static_cast<std::uint32_t>(rank);

        // 2. Lakes: connected filled depressions that are large or deep enough.
        std::vector<std::uint8_t> visited(tileCount, 0);
        std::vector<std::uint32_t> component;
        for (std::size_t start = 0; start < tileCount; ++start)
        {
            if (visited[start] || water[start] != SurfaceWater::Land || filled[start] <= elevation[start])
                continue;
            component.clear();
            component.push_back(static_cast<std::uint32_t>(start));
            visited[start] = 1;
            std::int32_t deepest = 0;
            std::uint32_t earliest = static_cast<std::uint32_t>(start);
            for (std::size_t head = 0; head < component.size(); ++head)
            {
                const std::uint32_t tile = component[head];
                deepest = std::max(deepest, filled[tile] - elevation[tile]);
                if (floodRank[tile] < floodRank[earliest])
                    earliest = tile;
                const WorldCoord coord = world.coordFromIndex(tile);
                for (std::size_t d = 0; d < kDirection8Count; ++d)
                {
                    const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
                    if (!world.isValid(n))
                        continue;
                    const std::size_t ni = world.index(n);
                    if (!visited[ni] && water[ni] == SurfaceWater::Land && filled[ni] > elevation[ni])
                    {
                        visited[ni] = 1;
                        component.push_back(static_cast<std::uint32_t>(ni));
                    }
                }
            }
            if (static_cast<std::int32_t>(component.size()) < settings.minLakeTiles && deepest < settings.minLakeDepthM)
                continue;

            Lake lake;
            lake.id = LakeId::fromIndex(hydrology.lakes.size());
            lake.surfaceElevation = static_cast<std::int16_t>(filled[start]);
            lake.tileCount = static_cast<std::uint32_t>(component.size());
            // The first lake tile reached by the flood was reached from the rim tile the lake spills over.
            lake.outlet = world.coordFromIndex(downstream(earliest));
            for (const std::uint32_t tile : component)
            {
                water[tile] = SurfaceWater::Lake;
                hydrology.lakeId[tile] = lake.id;
            }
            hydrology.lakes.push_back(lake);
        }

        // 3. Discharge: local runoff accumulated downstream (upstream tiles come later in flood order).
        const double tileAreaM2 = world.config().tileSizeMeters * world.config().tileSizeMeters;
        std::vector<double> accumulated(tileCount, 0.0);
        for (const std::uint32_t tile : floodOrder)
        {
            const float pet = potentialEvaporationMm(static_cast<float>(climate.meanAnnualTemperature[tile]) / 10.0f);
            const float runoff = runoffMm(static_cast<float>(climate.annualRainfall[tile]), pet);
            accumulated[tile] = static_cast<double>(runoff) / 1000.0 * tileAreaM2 / kSecondsPerYear;
        }
        for (auto it = floodOrder.rbegin(); it != floodOrder.rend(); ++it)
        {
            const std::size_t next = downstream(*it);
            if (next != kNone && water[next] != SurfaceWater::Ocean)
                accumulated[next] += accumulated[*it];
        }
        for (const std::uint32_t tile : floodOrder)
        {
            const double hundredths = std::floor(accumulated[tile] * 100.0 + 0.5);
            hydrology.discharge[tile] = static_cast<std::uint32_t>(std::min(hundredths, 4.0e9));
        }

        // 4. Rivers: land tiles above the stream threshold, grouped into main stems.
        const auto streamThreshold = static_cast<std::uint32_t>(std::floor(settings.streamDischarge * 100.0f + 0.5f));
        const auto isRiverTile = [&](std::size_t index)
        { return water[index] == SurfaceWater::Land && hydrology.discharge[index] >= streamThreshold; };

        // Largest upstream river or lake tile of each tile (ties: lowest index).
        std::vector<std::uint32_t> mainChild(tileCount, kNone);
        for (const std::uint32_t tile : floodOrder)
        {
            if (!isRiverTile(tile) && water[tile] != SurfaceWater::Lake)
                continue;
            const std::size_t next = downstream(tile);
            if (next == kNone || !isRiverTile(next))
                continue;
            const std::uint32_t current = mainChild[next];
            if (current == kNone || hydrology.discharge[tile] > hydrology.discharge[current] ||
                (hydrology.discharge[tile] == hydrology.discharge[current] && tile < current))
                mainChild[next] = tile;
        }

        std::vector<River> stems;
        for (std::size_t tile = 0; tile < tileCount; ++tile)
        {
            if (!isRiverTile(tile))
                continue;
            // A stem starts where no river flows in, or where a lake drains out.
            if (mainChild[tile] != kNone && water[mainChild[tile]] != SurfaceWater::Lake)
                continue;
            River river;
            std::size_t current = tile;
            for (;;)
            {
                river.path.push_back(world.coordFromIndex(current));
                const std::size_t next = downstream(current);
                if (next == kNone || !isRiverTile(next) || mainChild[next] != current)
                    break;
                current = next;
            }
            river.mouthDischarge = hydrology.discharge[current];
            stems.push_back(std::move(river));
        }
        // Largest rivers get the lowest ids.
        std::sort(stems.begin(), stems.end(), [&](const River &a, const River &b)
                  {
                      if (a.mouthDischarge != b.mouthDischarge)
                          return a.mouthDischarge > b.mouthDischarge;
                      return world.index(a.source()) < world.index(b.source()); });

        for (std::size_t r = 0; r < stems.size(); ++r)
        {
            stems[r].id = RiverId::fromIndex(r);
            for (const WorldCoord tile : stems[r].path)
                hydrology.riverId[world.index(tile)] = stems[r].id;
        }
        for (River &river : stems)
        {
            const std::size_t next = downstream(world.index(river.mouth()));
            if (next == kNone)
            {
                river.endsIn = RiverEnd::MapEdge;
            }
            else if (water[next] == SurfaceWater::Ocean)
            {
                river.endsIn = RiverEnd::Ocean;
            }
            else if (water[next] == SurfaceWater::Lake)
            {
                river.endsIn = RiverEnd::Lake;
                river.lake = hydrology.lakeId[next];
                hydrology.lakes[river.lake.index()].inflows.push_back(river.id);
            }
            else
            {
                river.endsIn = RiverEnd::River;
                river.flowsInto = hydrology.riverId[next];
                OLAM_ASSERT(river.flowsInto.isValid());
                stems[river.flowsInto.index()].tributaries.push_back(river.id);
            }
        }
        for (Lake &lake : hydrology.lakes)
            lake.outflow = hydrology.riverId[world.index(lake.outlet)];
        hydrology.rivers = std::move(stems);

        // 5. Floodplain strength (0..1) around rivers for soil and fertility; wider along larger rivers.
        Layer<std::uint8_t> riverTiles(width, height, 0);
        for (std::size_t i = 0; i < tileCount; ++i)
            riverTiles[i] = hydrology.riverId[i].isValid() ? 1 : 0;
        Layer<std::uint32_t> nearestRiver;
        const Layer<std::int32_t> distance = chamferDistance(riverTiles, &nearestRiver);
        const auto kmPerUnit = static_cast<float>(world.config().tileSizeMeters / 1000.0 / kChamferOrthogonal);
        Layer<float> &floodplain = context.createWorkingLayer(worldgen::kFloodplain);
        for (std::size_t i = 0; i < tileCount; ++i)
        {
            if (water[i] != SurfaceWater::Land || nearestRiver[i] == kNone)
                continue;
            const std::uint32_t river = nearestRiver[i];
            const float km = static_cast<float>(distance[i]) * kmPerUnit;
            const float dischargeM3s = static_cast<float>(hydrology.discharge[river]) / 100.0f;
            const float halfWidth = settings.floodplainBaseKm + settings.floodplainKmPerSqrtDischarge * std::sqrt(dischargeM3s);
            const float rise = static_cast<float>(std::max(0, elevation[i] - elevation[river]));
            floodplain[i] = clamp01(1.0f - km / std::max(halfWidth, 0.001f)) * clamp01(1.0f - rise / settings.floodplainMaxRiseM);
        }
    }

} // namespace olam
