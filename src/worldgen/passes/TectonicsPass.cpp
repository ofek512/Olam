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
            // Continent the plate belongs to; -1 for oceanic plates.
            int continent = -1;
        };

        std::vector<Plate> createPlates(const World &world, int count, std::uint64_t seed)
        {
            const TectonicsSettings &settings = world.config().generation.tectonics;
            Pcg32 rng(seed);
            std::vector<Plate> plates;
            plates.reserve(static_cast<std::size_t>(count));

            for (int i = 0; i < count; ++i)
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

        // The two plates nearest to a point and how that point relates to the boundary between them.
        struct BoundaryContact
        {
            std::size_t own = 0;
            std::size_t other = 0;
            float distanceKm = 0.0f;
            // Relative drift towards each other (> 0 converging, < 0 diverging).
            float converging = 0.0f;
        };

        BoundaryContact nearestBoundary(const std::vector<Plate> &plates, float px, float py, float tileKm)
        {
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
            BoundaryContact contact;
            contact.own = nearest;
            contact.other = second;
            // Distance from the point to the perpendicular bisector between both plate centres.
            contact.distanceKm = (separation > 0.0f ? (secondSq - nearestSq) / (2.0f * separation) : 0.0f) * tileKm;
            contact.converging =
                separation > 0.0f ? ((own.driftX - other.driftX) * nx + (own.driftY - other.driftY) * ny) / separation : 0.0f;
            return contact;
        }

        // Groups continental plates into a per-seed number of continents: well-spread seed plates each gather the
        // nearest remaining plates. Independent coin flips would merge neighbouring continental plates into one mass.
        void assignContinents(std::vector<Plate> &plates, const TectonicsSettings &settings, std::uint64_t seed)
        {
            Pcg32 rng(seed);
            const int plateCount = static_cast<int>(plates.size());
            const int continents = std::min(rng.nextInt(settings.continentCountMin, settings.continentCountMax), plateCount - 1);
            const int continental = std::clamp(
                static_cast<int>(std::floor(settings.continentalFraction * static_cast<float>(plateCount) + 0.5f)), continents,
                plateCount - 1);
            const auto distanceSq = [&](std::size_t a, std::size_t b)
            {
                const float dx = plates[a].x - plates[b].x;
                const float dy = plates[a].y - plates[b].y;
                return dx * dx + dy * dy;
            };

            for (Plate &plate : plates)
                plate.continent = -1;

            // Seeds: random first, then farthest-point sampling.
            std::vector<std::size_t> seeds = {rng.nextBounded(static_cast<std::uint32_t>(plateCount))};
            plates[seeds[0]].continent = 0;
            while (static_cast<int>(seeds.size()) < continents)
            {
                std::size_t best = 0;
                float bestDistance = -1.0f;
                for (std::size_t p = 0; p < plates.size(); ++p)
                {
                    if (plates[p].continent >= 0)
                        continue;
                    float nearest = 3.4e38f;
                    for (const std::size_t s : seeds)
                        nearest = std::min(nearest, distanceSq(p, s));
                    if (nearest > bestDistance)
                    {
                        bestDistance = nearest;
                        best = p;
                    }
                }
                plates[best].continent = static_cast<int>(seeds.size());
                seeds.push_back(best);
            }

            // Continents take turns adding the unassigned plate closest to any of their plates.
            for (int assigned = continents, turn = 0; assigned < continental; ++assigned, ++turn)
            {
                const int continent = turn % continents;
                std::size_t best = 0;
                float bestDistance = 3.4e38f;
                for (std::size_t p = 0; p < plates.size(); ++p)
                {
                    if (plates[p].continent >= 0)
                        continue;
                    for (std::size_t q = 0; q < plates.size(); ++q)
                    {
                        if (plates[q].continent == continent && distanceSq(p, q) < bestDistance)
                        {
                            bestDistance = distanceSq(p, q);
                            best = p;
                        }
                    }
                }
                plates[best].continent = continent;
            }

            for (Plate &plate : plates)
            {
                plate.continental = plate.continent >= 0;
                plate.baseHeight = plate.continental ? 0.55f + 0.10f * rng.nextFloat01() : 0.20f + 0.08f * rng.nextFloat01();
            }
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

        std::vector<Plate> plates = createPlates(world, settings.plateCount, seed);
        assignContinents(plates, settings, deriveSeed(seed, olam::seedId("CONTINEN")));
        const std::vector<Plate> paleoPlates =
            createPlates(world, settings.paleoPlateCount, deriveSeed(seed, olam::seedId("PALEO")));
        const std::uint64_t warpSeedX = deriveSeed(seed, olam::seedId("WARPX"));
        const std::uint64_t warpSeedY = deriveSeed(seed, olam::seedId("WARPY"));
        const std::uint64_t rockSeed = deriveSeed(seed, olam::seedId("ROCK"));
        const noise::FractalParams warpParams{4, 1.0f / settings.boundaryWarpWavelengthKm, 2.0f, 0.5f};
        const noise::FractalParams rockParams{3, 1.0f / 350.0f, 2.0f, 0.5f};
        const float warpTiles = settings.boundaryWarpKm / tileKm;
        noise::FractalSampler warpX(warpSeedX, warpParams);
        noise::FractalSampler warpY(warpSeedY, warpParams);
        noise::FractalSampler rockNoise(rockSeed, rockParams);

        auto &terrain = world.terrain();
        terrain.plateId.resize(width, height, 0);
        terrain.rockType.resize(width, height, RockType::Sedimentary);
        terrain.province.resize(width, height, GeologicalProvince::Count);
        Layer<float> &base = context.createWorkingLayer(worldgen::kPlateBase);
        Layer<float> &uplift = context.createWorkingLayer(worldgen::kUplift);
        Layer<float> &rift = context.createWorkingLayer(worldgen::kRift);
        Layer<float> &ancient = context.createWorkingLayer(worldgen::kAncientBelt);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::size_t i = terrain.plateId.index(x, y);
                const float kmX = (static_cast<float>(x) + 0.5f) * tileKm;
                const float kmY = (static_cast<float>(y) + 0.5f) * tileKm;
                const float px = static_cast<float>(x) + 0.5f + warpTiles * warpX.fbm(kmX, kmY);
                const float py = static_cast<float>(y) + 0.5f + warpTiles * warpY.fbm(kmX, kmY);

                const BoundaryContact contact = nearestBoundary(plates, px, py, tileKm);
                const Plate &own = plates[contact.own];
                const Plate &other = plates[contact.other];
                const float distance = contact.distanceKm;
                const float converging = contact.converging;
                const int contacts = (own.continental ? 1 : 0) + (other.continental ? 1 : 0);
                // Different continents meeting: keep an ocean between them instead of welding them together.
                const bool separateContinents = contacts == 2 && own.continent != other.continent;

                base[i] = own.baseHeight;
                if (separateContinents)
                {
                    base[i] = lerp(0.24f, own.baseHeight, smoothstep(0.0f, settings.continentGapKm, distance));
                }
                else if (converging > 0.0f)
                {
                    // Ocean-ocean convergence makes island arcs rather than full ranges.
                    const float kind = contacts > 0 ? 1.0f : 0.6f;
                    uplift[i] = converging * kind * (1.0f - smoothstep(0.0f, settings.mountainWidthKm, distance));
                }
                else
                {
                    rift[i] = -converging * (1.0f - smoothstep(0.0f, settings.riftWidthKm, distance));
                }
                terrain.plateId[i] = static_cast<std::uint8_t>(contact.own);

                // Collision zones of the older cycle; restricted to continental crust once it is smoothed.
                const BoundaryContact paleo = nearestBoundary(paleoPlates, px, py, tileKm);
                if (paleo.converging > 0.0f)
                    ancient[i] = std::min(1.0f, 2.0f * paleo.converging) *
                                 (1.0f - smoothstep(0.0f, settings.ancientBeltWidthKm, paleo.distanceKm));

                // Active belts first; plate interiors are decided after the crust field is smoothed.
                RockType rock = RockType::Count;
                const bool inBelt = distance < settings.beltWidthKm && !separateContinents;
                if (inBelt && converging > 0.2f)
                {
                    rock = contacts == 2 ? RockType::Metamorphic : RockType::Igneous;
                    terrain.province[i] = GeologicalProvince::ActiveOrogen;
                }
                else if (inBelt && converging < -0.2f)
                {
                    rock = RockType::Igneous;
                    terrain.province[i] = GeologicalProvince::Rift;
                }
                terrain.rockType[i] = rock;
            }
        }

        // Smooths the per-plate step functions into continuous fields (no seams at triple junctions).
        const auto tilesFor = [tileKm](float km)
        { return std::max(1, static_cast<int>(km / tileKm * 0.5f)); };
        boxBlur(base, tilesFor(settings.boundaryBlendKm), 3);
        boxBlur(uplift, tilesFor(settings.mountainWidthKm * 0.25f), 2);
        boxBlur(rift, tilesFor(settings.riftWidthKm * 0.25f), 2);
        boxBlur(ancient, tilesFor(settings.ancientBeltWidthKm * 0.25f), 2);

        // Thick (continental) crust: basins, shields and ancient orogens; thin oceanic crust: igneous.
        constexpr float kContinentalCrust = 0.42f;
        constexpr float kAncientOrogen = 0.3f;
        noise::FractalSampler massifNoise(deriveSeed(seed, olam::seedId("MASSIF")),
                                          {2, 1.0f / settings.ancientMassifWavelengthKm, 2.0f, 0.5f});
        noise::FractalSampler graniteNoise(deriveSeed(seed, olam::seedId("GRANITE")),
                                           {2, 1.0f / settings.graniteWavelengthKm, 2.0f, 0.5f});
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::size_t i = base.index(x, y);
                const float kmX = (static_cast<float>(x) + 0.5f) * tileKm;
                const float kmY = (static_cast<float>(y) + 0.5f) * tileKm;

                // Old belts survive as separate massifs (Bohemian Massif, Harz, Cornwall) rather than a continuous line.
                if (ancient[i] > 0.0f)
                    ancient[i] *= smoothstep(0.38f, 0.46f, base[i]) * smoothstep(-0.35f, 0.15f, massifNoise.fbm(kmX, kmY));

                GeologicalProvince province = terrain.province[i];
                if (province == GeologicalProvince::Count)
                {
                    if (base[i] < kContinentalCrust)
                        province = GeologicalProvince::Oceanic;
                    else if (ancient[i] >= kAncientOrogen)
                        province = GeologicalProvince::AncientOrogen;
                    else if (rockNoise.fbm(kmX, kmY) > 0.2f)
                        province = GeologicalProvince::Shield;
                    else
                        province = GeologicalProvince::Basin;
                    terrain.province[i] = province;
                }

                // Granite intrusions only matter in crystalline provinces.
                const auto granite = [&]
                { return graniteNoise.fbm(kmX, kmY) > settings.graniteThreshold; };
                switch (province)
                {
                case GeologicalProvince::Oceanic:
                case GeologicalProvince::Rift:
                    terrain.rockType[i] = RockType::Igneous;
                    break;
                case GeologicalProvince::ActiveOrogen:
                    if (granite())
                        terrain.rockType[i] = RockType::Igneous;
                    break;
                case GeologicalProvince::AncientOrogen:
                case GeologicalProvince::Shield:
                    terrain.rockType[i] = granite() ? RockType::Igneous : RockType::Metamorphic;
                    break;
                case GeologicalProvince::Basin:
                case GeologicalProvince::Count:
                    terrain.rockType[i] = RockType::Sedimentary;
                    break;
                }
            }
        }
    }

} // namespace olam
