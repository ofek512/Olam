#include "worldgen/passes/TectonicsPass.h"

#include "core/math/MathUtil.h"
#include "core/noise/Noise.h"
#include "core/random/Pcg32.h"
#include "core/random/Seed.h"
#include "world/World.h"
#include "worldgen/WorkingLayers.h"
#include "worldgen/WorldGenContext.h"
#include "worldgen/util/Blur.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace olam
{

    namespace
    {

        struct Plate
        {
            float x;
            float y;
            float driftX;
            float driftY;
            float baseHeight;
            bool continental;
        };

        std::vector<Plate> createPlates(const World &world, std::uint64_t seed)
        {
            const TectonicsSettings &settings = world.config().generation.tectonics;
            Pcg32 rng(seed);
            std::vector<Plate> plates;
            plates.reserve(static_cast<std::size_t>(settings.plateCount));

            for (int i = 0; i < settings.plateCount; ++i)
            {
                Plate plate{};
                plate.x = rng.nextFloat01() * static_cast<float>(world.width());
                plate.y = rng.nextFloat01() * static_cast<float>(world.height());
                plate.continental = rng.nextDouble01() < settings.continentalFraction;

                // Uniform direction by rejection sampling in the unit disc (no trigonometry).
                float dx = 0.0f;
                float dy = 0.0f;
                float lengthSq = 0.0f;
                do
                {
                    dx = rng.nextFloat01() * 2.0f - 1.0f;
                    dy = rng.nextFloat01() * 2.0f - 1.0f;
                    lengthSq = dx * dx + dy * dy;
                } while (lengthSq > 1.0f || lengthSq < 0.01f);
                const float speed = 0.3f + 0.7f * rng.nextFloat01();
                const float length = std::sqrt(lengthSq);
                plate.driftX = dx / length * speed;
                plate.driftY = dy / length * speed;

                plate.baseHeight = plate.continental ? 0.55f + 0.10f * rng.nextFloat01() : 0.20f + 0.08f * rng.nextFloat01();
                plates.push_back(plate);
            }

            // Guarantee both continents and oceans exist.
            bool anyContinental = false;
            bool anyOceanic = false;
            for (const Plate &plate : plates)
                (plate.continental ? anyContinental : anyOceanic) = true;
            if (!anyContinental)
            {
                plates.front().continental = true;
                plates.front().baseHeight = 0.6f;
            }
            if (!anyOceanic)
            {
                plates.back().continental = false;
                plates.back().baseHeight = 0.24f;
            }
            return plates;
        }

    } // namespace

    void TectonicsPass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const TectonicsSettings &settings = world.config().generation.tectonics;
        const std::uint64_t seed = context.seedFor(*this);
        const int width = world.width();
        const int height = world.height();
        const auto tileKm = static_cast<float>(world.config().tileSizeMeters / 1000.0);

        const std::vector<Plate> plates = createPlates(world, seed);
        const std::uint64_t warpSeedX = deriveSeed(seed, olam::seedId("WARPX"));
        const std::uint64_t warpSeedY = deriveSeed(seed, olam::seedId("WARPY"));
        const std::uint64_t rockSeed = deriveSeed(seed, olam::seedId("ROCK"));
        const noise::FractalParams warpParams{4, 1.0f / settings.boundaryWarpWavelengthKm, 2.0f, 0.5f};
        const noise::FractalParams rockParams{3, 1.0f / 350.0f, 2.0f, 0.5f};
        const float warpTiles = settings.boundaryWarpKm / tileKm;

        auto &terrain = world.terrain();
        terrain.plateId.resize(width, height, 0);
        terrain.rockType.resize(width, height, RockType::Sedimentary);
        Layer<float> &base = context.createWorkingLayer(worldgen::kPlateBase);
        Layer<float> &uplift = context.createWorkingLayer(worldgen::kUplift);
        Layer<float> &rift = context.createWorkingLayer(worldgen::kRift);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::size_t i = terrain.plateId.index(x, y);
                const float kmX = (static_cast<float>(x) + 0.5f) * tileKm;
                const float kmY = (static_cast<float>(y) + 0.5f) * tileKm;
                const float px = static_cast<float>(x) + 0.5f + warpTiles * noise::fbm(warpSeedX, kmX, kmY, warpParams);
                const float py = static_cast<float>(y) + 0.5f + warpTiles * noise::fbm(warpSeedY, kmX, kmY, warpParams);

                std::size_t nearest = 0;
                std::size_t second = 1;
                float nearestSq = 3.4e38f;
                float secondSq = 3.4e38f;
                for (std::size_t p = 0; p < plates.size(); ++p)
                {
                    const float dx = px - plates[p].x;
                    const float dy = py - plates[p].y;
                    const float distSq = dx * dx + dy * dy;
                    if (distSq < nearestSq)
                    {
                        second = nearest;
                        secondSq = nearestSq;
                        nearest = p;
                        nearestSq = distSq;
                    }
                    else if (distSq < secondSq)
                    {
                        second = p;
                        secondSq = distSq;
                    }
                }

                const Plate &own = plates[nearest];
                const Plate &other = plates[second];
                const float nx = other.x - own.x;
                const float ny = other.y - own.y;
                const float separation = std::sqrt(nx * nx + ny * ny);
                // Distance from the point to the perpendicular bisector between both plate centres.
                const float distanceTiles = separation > 0.0f ? (secondSq - nearestSq) / (2.0f * separation) : 0.0f;
                const float distance = distanceTiles * tileKm;
                const float converging = separation > 0.0f
                                             ? ((own.driftX - other.driftX) * nx + (own.driftY - other.driftY) * ny) / separation
                                             : 0.0f;
                const int contacts = (own.continental ? 1 : 0) + (other.continental ? 1 : 0);

                base[i] = own.baseHeight;
                if (converging > 0.0f)
                {
                    // Ocean-ocean convergence makes island arcs rather than full ranges.
                    const float kind = contacts > 0 ? 1.0f : 0.6f;
                    uplift[i] = converging * kind * (1.0f - smoothstep(0.0f, settings.mountainWidthKm, distance));
                }
                else
                {
                    rift[i] = -converging * (1.0f - smoothstep(0.0f, settings.riftWidthKm, distance));
                }
                terrain.plateId[i] = static_cast<std::uint8_t>(nearest);

                // Active belts first; plate interiors are decided after the crust field is smoothed.
                RockType rock = RockType::Count;
                const bool inBelt = distance < settings.beltWidthKm;
                if (inBelt && converging > 0.2f)
                    rock = contacts == 2 ? RockType::Metamorphic : RockType::Igneous;
                else if (inBelt && converging < -0.2f)
                    rock = RockType::Igneous;
                terrain.rockType[i] = rock;
            }
        }

        // Smooths the per-plate step functions into continuous fields (no seams at triple junctions).
        const auto tilesFor = [tileKm](float km)
        { return std::max(1, static_cast<int>(km / tileKm * 0.5f)); };
        boxBlur(base, tilesFor(settings.boundaryBlendKm), 3);
        boxBlur(uplift, tilesFor(settings.mountainWidthKm * 0.25f), 2);
        boxBlur(rift, tilesFor(settings.riftWidthKm * 0.25f), 2);

        // Thick (continental) crust: sedimentary basins and metamorphic shields; thin oceanic crust: igneous.
        constexpr float kContinentalCrust = 0.42f;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::size_t i = base.index(x, y);
                if (terrain.rockType[i] != RockType::Count)
                    continue;
                if (base[i] < kContinentalCrust)
                {
                    terrain.rockType[i] = RockType::Igneous;
                    continue;
                }
                const float kmX = (static_cast<float>(x) + 0.5f) * tileKm;
                const float kmY = (static_cast<float>(y) + 0.5f) * tileKm;
                terrain.rockType[i] =
                    noise::fbm(rockSeed, kmX, kmY, rockParams) > 0.2f ? RockType::Metamorphic : RockType::Sedimentary;
            }
        }
    }

} // namespace olam
