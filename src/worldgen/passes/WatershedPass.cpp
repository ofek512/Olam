#include "worldgen/passes/WatershedPass.h"

#include "world/World.h"
#include "worldgen/WorldGenContext.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace olam
{

    namespace
    {

        constexpr std::uint32_t kNone = std::numeric_limits<std::uint32_t>::max();

        struct UnionFind
        {
            std::vector<std::uint32_t> parent;
            std::vector<std::uint32_t> size;

            std::uint32_t find(std::uint32_t a)
            {
                while (parent[a] != a)
                {
                    parent[a] = parent[parent[a]];
                    a = parent[a];
                }
                return a;
            }
        };

    } // namespace

    std::optional<std::string> WatershedPass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().hydrology().flowDirection.empty())
            return std::string("requires HydrologyPass");
        return std::nullopt;
    }

    void WatershedPass::run(WorldGenContext &context)
    {
        World &world = context.world();
        HydrologyData &hydrology = world.hydrology();
        const auto &water = hydrology.surfaceWater;
        const auto &flow = hydrology.flowDirection;
        const std::size_t tileCount = world.tileCount();
        const int width = world.width();
        const int height = world.height();

        const auto downstream = [&](std::size_t index) -> std::uint32_t
        {
            if (flow[index] == kNoFlow)
                return kNone;
            const WorldCoord next = neighbor(world.coordFromIndex(index), flow[index]);
            if (!world.isValid(next))
                return kNone;
            const std::size_t n = world.index(next);
            return water[n] == SurfaceWater::Ocean ? kNone : static_cast<std::uint32_t>(n);
        };

        // 1. Outlet of every land / lake tile: the last tile before the sea or the map edge.
        std::vector<std::uint32_t> outlet(tileCount, kNone);
        std::vector<std::uint32_t> path;
        for (std::size_t start = 0; start < tileCount; ++start)
        {
            if (water[start] == SurfaceWater::Ocean || outlet[start] != kNone)
                continue;
            path.clear();
            std::uint32_t current = static_cast<std::uint32_t>(start);
            std::uint32_t root = kNone;
            for (;;)
            {
                if (outlet[current] != kNone)
                {
                    root = outlet[current];
                    break;
                }
                path.push_back(current);
                const std::uint32_t next = downstream(current);
                if (next == kNone)
                {
                    root = current;
                    break;
                }
                current = next;
            }
            for (const std::uint32_t tile : path)
                outlet[tile] = root;
        }

        // 2. Raw catchments, numbered in outlet index order.
        std::vector<std::uint32_t> catchmentOfOutlet(tileCount, kNone);
        std::vector<std::uint32_t> catchmentOutlet;
        for (std::size_t i = 0; i < tileCount; ++i)
        {
            if (outlet[i] == i)
            {
                catchmentOfOutlet[i] = static_cast<std::uint32_t>(catchmentOutlet.size());
                catchmentOutlet.push_back(static_cast<std::uint32_t>(i));
            }
        }
        const std::size_t catchments = catchmentOutlet.size();
        std::vector<std::uint32_t> catchment(tileCount, kNone);
        UnionFind groups;
        groups.parent.resize(catchments);
        groups.size.assign(catchments, 0);
        for (std::uint32_t c = 0; c < catchments; ++c)
            groups.parent[c] = c;
        for (std::size_t i = 0; i < tileCount; ++i)
        {
            if (outlet[i] == kNone)
                continue;
            catchment[i] = catchmentOfOutlet[outlet[i]];
            ++groups.size[catchment[i]];
        }

        // 3. Shared border length between neighbouring catchments (4-neighbourhood, land only).
        std::vector<std::uint64_t> borderPairs;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::size_t i = world.index({x, y});
                if (catchment[i] == kNone)
                    continue;
                const auto addPair = [&](std::size_t n)
                {
                    if (catchment[n] == kNone || catchment[n] == catchment[i])
                        return;
                    const std::uint64_t a = std::min(catchment[i], catchment[n]);
                    const std::uint64_t b = std::max(catchment[i], catchment[n]);
                    borderPairs.push_back((a << 32) | b);
                };
                if (x + 1 < width)
                    addPair(i + 1);
                if (y + 1 < height)
                    addPair(i + static_cast<std::size_t>(width));
            }
        }
        std::sort(borderPairs.begin(), borderPairs.end());
        struct Border
        {
            std::uint32_t other;
            std::uint32_t length;
        };
        std::vector<std::vector<Border>> borders(catchments);
        for (std::size_t p = 0; p < borderPairs.size();)
        {
            std::size_t q = p;
            while (q < borderPairs.size() && borderPairs[q] == borderPairs[p])
                ++q;
            const auto a = static_cast<std::uint32_t>(borderPairs[p] >> 32);
            const auto b = static_cast<std::uint32_t>(borderPairs[p] & 0xFFFFFFFFu);
            const auto length = static_cast<std::uint32_t>(q - p);
            borders[a].push_back({b, length});
            borders[b].push_back({a, length});
            p = q;
        }

        // 4. Small catchments (coastal strips, short streams) join the neighbour with the longest shared border.
        const double tileKm2 = world.config().tileSizeMeters * world.config().tileSizeMeters / 1.0e6;
        const auto minTiles = static_cast<std::uint32_t>(
            std::ceil(world.config().generation.hydrology.minWatershedKm2 / tileKm2));
        std::vector<std::uint32_t> bySize(catchments);
        for (std::uint32_t c = 0; c < catchments; ++c)
            bySize[c] = c;
        std::stable_sort(bySize.begin(), bySize.end(), [&](std::uint32_t a, std::uint32_t b)
                         { return groups.size[a] < groups.size[b]; });
        std::vector<std::pair<std::uint32_t, std::uint32_t>> candidates;
        // Merging can leave a small group whose small members were handled earlier; repeat until stable.
        for (bool merged = true; merged;)
        {
            merged = false;
            for (const std::uint32_t c : bySize)
            {
                const std::uint32_t root = groups.find(c);
                if (groups.size[root] >= minTiles)
                    continue;
                candidates.clear();
                for (const Border &border : borders[c])
                {
                    const std::uint32_t other = groups.find(border.other);
                    if (other == root)
                        continue;
                    auto it = std::find_if(candidates.begin(), candidates.end(), [&](const auto &entry)
                                           { return entry.first == other; });
                    if (it == candidates.end())
                        candidates.push_back({other, border.length});
                    else
                        it->second += border.length;
                }
                if (candidates.empty())
                    continue; // an island smaller than the minimum stays a basin of its own
                std::uint32_t best = candidates.front().first;
                std::uint32_t bestLength = candidates.front().second;
                for (const auto &[other, length] : candidates)
                {
                    if (length > bestLength || (length == bestLength && other < best))
                    {
                        best = other;
                        bestLength = length;
                    }
                }
                groups.parent[root] = best;
                groups.size[best] += groups.size[root];
                merged = true;
            }
        }

        // 5. One watershed per group; the largest-discharge outlet is its main outlet. Largest basins get low ids.
        struct Group
        {
            std::uint32_t root;
            std::uint32_t tiles;
            std::uint32_t mainOutlet;
            std::uint64_t discharge;
        };
        std::vector<std::uint32_t> groupIndex(catchments, kNone);
        std::vector<Group> found;
        for (std::uint32_t c = 0; c < catchments; ++c)
        {
            const std::uint32_t root = groups.find(c);
            if (groupIndex[root] == kNone)
            {
                groupIndex[root] = static_cast<std::uint32_t>(found.size());
                found.push_back({root, groups.size[root], catchmentOutlet[c], 0});
            }
            Group &group = found[groupIndex[root]];
            const std::uint32_t tile = catchmentOutlet[c];
            group.discharge += hydrology.discharge[tile];
            if (hydrology.discharge[tile] > hydrology.discharge[group.mainOutlet])
                group.mainOutlet = tile;
        }
        std::vector<std::uint32_t> order(found.size());
        for (std::uint32_t g = 0; g < order.size(); ++g)
            order[g] = g;
        std::sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b)
                  {
                      if (found[a].tiles != found[b].tiles)
                          return found[a].tiles > found[b].tiles;
                      return found[a].mainOutlet < found[b].mainOutlet; });

        hydrology.watersheds.clear();
        hydrology.watersheds.resize(found.size());
        std::vector<WatershedId> idOfGroup(found.size());
        for (std::size_t rank = 0; rank < order.size(); ++rank)
        {
            const Group &group = found[order[rank]];
            Watershed &watershed = hydrology.watersheds[rank];
            watershed.id = WatershedId::fromIndex(rank);
            watershed.outlet = world.coordFromIndex(group.mainOutlet);
            watershed.tileCount = group.tiles;
            watershed.outletDischarge = static_cast<std::uint32_t>(std::min<std::uint64_t>(group.discharge, 0xFFFFFFFFu));
            idOfGroup[order[rank]] = watershed.id;
        }

        hydrology.watershedId.resize(width, height, WatershedId{});
        for (std::size_t i = 0; i < tileCount; ++i)
        {
            if (catchment[i] != kNone)
                hydrology.watershedId[i] = idOfGroup[groupIndex[groups.find(catchment[i])]];
        }

        // Rivers are listed largest first, so the first river ending in a basin is its main river.
        for (const River &river : hydrology.rivers)
        {
            Watershed &watershed = hydrology.watersheds[hydrology.watershedId[world.index(river.mouth())].index()];
            if (!watershed.mainRiver.isValid())
                watershed.mainRiver = river.id;
            watershed.rivers.push_back(river.id);
        }
        for (const Lake &lake : hydrology.lakes)
            hydrology.watersheds[hydrology.watershedId[world.index(lake.outlet)].index()].lakes.push_back(lake.id);
    }

} // namespace olam
