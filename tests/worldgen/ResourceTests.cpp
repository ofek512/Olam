#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "world/WorldStats.h"
#include "world/queries/ResourceQueries.h"
#include "world/queries/TerrainQueries.h"

#include <cstdio>
#include <vector>

using namespace olam;

namespace
{

    bool nextToLake(const World &world, WorldCoord coord)
    {
        for (std::size_t d = 0; d < kDirection8Count; ++d)
        {
            const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
            if (world.isValid(n) && world.hydrology().surfaceWater[world.index(n)] == SurfaceWater::Lake)
                return true;
        }
        return false;
    }

} // namespace

OLAM_TEST(resource_deposits_are_consistent)
{
    const auto world = test::generateWorld(42, test::smallWorldConfig(512, 256));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    const ResourceData &resources = world->resources();
    const WorldGenSettings &settings = world->config().generation;
    OLAM_CHECK(!resources.deposits.empty());

    std::vector<std::uint32_t> tiles(resources.deposits.size(), 0);
    for (std::size_t i = 0; i < world->tileCount(); ++i)
    {
        const DepositId id = resources.depositId[i];
        if (!id.isValid())
            continue;
        OLAM_CHECK(id.index() < resources.deposits.size());
        OLAM_CHECK(world->hydrology().surfaceWater[i] == SurfaceWater::Land);
        ++tiles[id.index()];
    }

    const float spacing = settings.resources.minSpacingKm / static_cast<float>(world->config().tileSizeMeters / 1000.0);
    for (std::size_t d = 0; d < resources.deposits.size(); ++d)
    {
        const Deposit &deposit = resources.deposits[d];
        const std::size_t center = world->index(deposit.center);
        OLAM_CHECK(deposit.id == DepositId::fromIndex(d));
        OLAM_CHECK(deposit.mineral < MineralType::Count && deposit.origin < DepositOrigin::Count);
        OLAM_CHECK(deposit.tileCount == tiles[d]);
        OLAM_CHECK(deposit.tileCount >= 1 && deposit.tileCount <= 20);
        OLAM_CHECK(deposit.richness >= 1 && deposit.richness <= 100);
        OLAM_CHECK(resources.depositId[center] == deposit.id);

        // Each origin only occurs in its geological / environmental setting.
        const GeologicalProvince province = world->terrain().province[center];
        switch (deposit.origin)
        {
        case DepositOrigin::Vein:
            OLAM_CHECK(province != GeologicalProvince::Oceanic && deposit.mineral != MineralType::Coal &&
                       deposit.mineral != MineralType::Salt);
            break;
        case DepositOrigin::Placer:
            OLAM_CHECK(deposit.mineral == MineralType::Gold || deposit.mineral == MineralType::Tin);
            OLAM_CHECK(world->hydrology().riverId[center].isValid());
            break;
        case DepositOrigin::Bog:
        {
            const bool wet = world->geography().biome[center] == Biome::Wetland ||
                             world->geography().soil[center] == SoilType::Peat ||
                             world->geography().soil[center] == SoilType::Alluvial || nextToLake(*world, deposit.center);
            OLAM_CHECK(deposit.mineral == MineralType::Iron && wet);
            break;
        }
        case DepositOrigin::Bedded:
            OLAM_CHECK(deposit.mineral == MineralType::Coal || deposit.mineral == MineralType::Iron);
            OLAM_CHECK(province == GeologicalProvince::Basin || province == GeologicalProvince::Shield);
            break;
        case DepositOrigin::Evaporite:
            OLAM_CHECK(deposit.mineral == MineralType::Salt &&
                       (province == GeologicalProvince::Basin || province == GeologicalProvince::ActiveOrogen));
            break;
        case DepositOrigin::SaltPan:
            OLAM_CHECK(deposit.mineral == MineralType::Salt);
            break;
        case DepositOrigin::Count:
            OLAM_CHECK(false);
            break;
        }

        // Same mineral and origin keep their minimum spacing (placers follow their own spacing along rivers).
        if (deposit.origin == DepositOrigin::Placer)
            continue;
        for (std::size_t other = d + 1; other < resources.deposits.size(); ++other)
        {
            const Deposit &b = resources.deposits[other];
            if (b.mineral != deposit.mineral || b.origin != deposit.origin)
                continue;
            const auto dx = static_cast<float>(b.center.x - deposit.center.x);
            const auto dy = static_cast<float>(b.center.y - deposit.center.y);
            OLAM_CHECK(dx * dx + dy * dy >= spacing * spacing);
        }
    }
}

OLAM_TEST(ores_are_not_confined_to_mountains)
{
    // Plate counts are fixed per map, so only larger maps have realistic basin / orogen proportions.
    const auto world = test::generateWorld(7, test::smallWorldConfig(1024, 1024));
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    std::size_t veins = 0;
    std::size_t onMountains = 0;
    for (const Deposit &deposit : world->resources().deposits)
    {
        if (deposit.origin != DepositOrigin::Vein || deposit.mineral == MineralType::Iron)
            continue;
        ++veins;
        onMountains += terrainClassAt(*world, deposit.center) == TerrainClass::Mountains ? 1u : 0u;
    }
    const DepositCoverage coverage = depositCoverage(*world, 64.0);
    const double iron = coverage.mineral[static_cast<std::size_t>(MineralType::Iron)];
    const double coal = coverage.mineral[static_cast<std::size_t>(MineralType::Coal)];
    const double salt = coverage.mineral[static_cast<std::size_t>(MineralType::Salt)];
    std::printf("  coverage of 64 km blocks: iron %.2f other metal %.2f coal %.2f salt %.2f; vein metals on mountains %zu/%zu\n",
                iron, coverage.nonIronMetal, coal, salt, onMountains, veins);
    OLAM_CHECK(veins > 0 && onMountains * 2 <= veins);
    OLAM_CHECK(iron >= 0.55);
    OLAM_CHECK(coverage.nonIronMetal >= 0.25);
    OLAM_CHECK(coal >= 0.15);
    OLAM_CHECK(salt >= 0.1);
}

OLAM_TEST(local_materials_in_range)
{
    const auto world = test::generateWorld(42);
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    double stone = 0.0;
    double clay = 0.0;
    std::size_t land = 0;
    for (int y = 0; y < world->height(); y += 2)
    {
        for (int x = 0; x < world->width(); x += 2)
        {
            const LocalMaterials materials = localMaterialsAt(*world, {x, y});
            OLAM_CHECK(materials.stone >= 0.0f && materials.stone <= 1.0f && materials.clay >= 0.0f && materials.clay <= 1.0f);
            if (world->hydrology().surfaceWater[world->index({x, y})] != SurfaceWater::Land)
                continue;
            ++land;
            stone += materials.stone;
            clay += materials.clay;
        }
    }
    // Basic building materials exist almost everywhere.
    OLAM_CHECK(land > 0 && stone / static_cast<double>(land) > 0.25 && clay / static_cast<double>(land) > 0.2);
}

OLAM_TEST(biological_yields_in_range)
{
    const auto world = test::generateWorld(42);
    OLAM_CHECK(world != nullptr);
    if (!world)
        return;
    bool anyFish = false;
    for (int y = 0; y < world->height(); y += 3)
    {
        for (int x = 0; x < world->width(); x += 3)
        {
            const BiologicalYields yields = biologicalYieldsAt(*world, {x, y});
            OLAM_CHECK(yields.wood >= 0.0f && yields.wood <= 1.0f);
            OLAM_CHECK(yields.game >= 0.0f && yields.game <= 1.0f);
            OLAM_CHECK(yields.fish >= 0.0f && yields.fish <= 1.0f);
            anyFish |= yields.fish > 0.0f;
        }
    }
    OLAM_CHECK(anyFish);
}
