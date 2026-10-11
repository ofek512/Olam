#include "TestFramework.h"
#include "worldgen/WorldGenTestUtil.h"

#include "settlement/LocalMapGenerator.h"
#include "settlement/ResourceState.h"
#include "settlement/SiteSelection.h"
#include "tools/settlement_viewer/LocalSprites.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <span>

using namespace olam;

namespace
{

    // A wooded local map with forage of several kinds (renewable and deadwood).
    std::unique_ptr<SettlementMap> forestMap()
    {
        static const auto world = test::generateWorld(42, test::smallWorldConfig(256, 256));
        SettlementConfig config;
        config.width = 512;
        config.height = 512;
        for (int y = 16; y < world->height() - 16; y += 5)
        {
            for (int x = 16; x < world->width() - 16; x += 5)
            {
                if (siteError(*world, {x, y}, config) || world->geography().treeCover[world->index({x, y})] < 50)
                    continue;
                auto map = generateLocalMap(*world, {x, y}, config).map;
                const ForageData &forage = map->forage();
                const bool deadwood = std::find(forage.kind.begin(), forage.kind.end(), ForageKind::Deadwood) != forage.kind.end();
                const bool renewable = std::any_of(forage.kind.begin(), forage.kind.end(), [](ForageKind kind)
                                                   { return forageInfo(kind).renewable; });
                if (deadwood && renewable && map->trees().size() > 0)
                    return map;
            }
        }
        return nullptr;
    }

    std::size_t firstForage(const SettlementMap &map, bool renewable)
    {
        for (std::size_t f = 0; f < map.forage().size(); ++f)
        {
            if (forageInfo(map.forage().kind[f]).renewable == renewable)
                return f;
        }
        return map.forage().size();
    }

} // namespace

OLAM_TEST(tree_wood_grows_with_size_and_kind)
{
    OLAM_CHECK(treeWoodKg(TreeKind::Broadleaf, 255) > treeWoodKg(TreeKind::Broadleaf, 100));
    OLAM_CHECK(treeWoodKg(TreeKind::Broadleaf, 255) > treeWoodKg(TreeKind::SmallTree, 255));
    OLAM_CHECK(treeWoodKg(TreeKind::Conifer, 0) > 0.0f);
}

OLAM_TEST(harvest_reduces_forage_and_regrowth_restores_it)
{
    const auto map = forestMap();
    OLAM_CHECK(map != nullptr);
    if (!map)
        return;
    const std::uint64_t generatedHash = hashSettlementMap(*map);
    ResourceState state;
    const std::size_t berries = firstForage(*map, true);
    const std::size_t deadwood = firstForage(*map, false);
    OLAM_CHECK(berries < map->forage().size() && deadwood < map->forage().size());
    const float maximum = map->forage().kilograms(berries);

    OLAM_CHECK(state.forageRemaining(*map, berries) == maximum);
    const float taken = state.harvestForage(*map, berries, 0.5f);
    OLAM_CHECK(taken == std::min(0.5f, maximum));
    OLAM_CHECK(std::abs(state.forageRemaining(*map, berries) - (maximum - taken)) < 1e-4f);
    // Never more than is left; then the patch is empty.
    OLAM_CHECK(state.harvestForage(*map, berries, 1.0e6f) <= maximum);
    OLAM_CHECK(state.forageDepleted(*map, berries));
    OLAM_CHECK(state.harvestForage(*map, berries, 1.0f) == 0.0f);

    // A year restores renewable forage; deadwood comes back only slowly.
    const float deadwoodMax = map->forage().kilograms(deadwood);
    state.harvestForage(*map, deadwood, 1.0e6f);
    state.regrow(*map, 1.0f);
    OLAM_CHECK(state.forageRemaining(*map, berries) == maximum);
    const float deadwoodLeft = state.forageRemaining(*map, deadwood);
    OLAM_CHECK(deadwoodLeft > 0.0f && deadwoodLeft < 0.1f * deadwoodMax);

    // Gameplay state never changes the generated map.
    OLAM_CHECK(hashSettlementMap(*map) == generatedHash);
}

OLAM_TEST(felled_trees_yield_wood_once_and_disappear)
{
    const auto map = forestMap();
    OLAM_CHECK(map != nullptr);
    if (!map)
        return;
    ResourceState state;
    const TreeData &trees = map->trees();
    // The tree nearest the map centre, and a view around it.
    std::size_t t = 0;
    for (std::size_t i = 0; i < trees.size(); ++i)
    {
        if (std::abs(trees.x[i] - 256) + std::abs(trees.y[i] - 256) < std::abs(trees.x[t] - 256) + std::abs(trees.y[t] - 256))
            t = i;
    }
    const TreeId tree = TreeId::fromIndex(t);
    const Rect view{static_cast<float>(trees.x[t]) - 20.0f, static_cast<float>(trees.y[t]) - 20.0f, 40.0f, 40.0f};
    const auto crowns = [&](const ResourceState *s)
    {
        std::vector<LocalSpriteInstance> sprites;
        collectLocalSprites(*map, view, 5.0f, true, sprites, s);
        return std::count_if(sprites.begin(), sprites.end(), [](const LocalSpriteInstance &sprite)
                             { return sprite.sprite < kFirstRockSprite; });
    };
    const auto before = crowns(&state);

    const float wood = state.fellTree(*map, tree);
    OLAM_CHECK(wood == treeWoodKg(trees.kind[t], trees.growth[t]) && wood > 0.0f);
    OLAM_CHECK(state.treeFelled(*map, tree));
    OLAM_CHECK(state.fellTree(*map, tree) == 0.0f);
    OLAM_CHECK(crowns(&state) == before - 1);
    // Regrowth does not bring felled trees back.
    state.regrow(*map, 10.0f);
    OLAM_CHECK(state.treeFelled(*map, tree));
}

OLAM_TEST(resource_state_round_trips_and_checks_its_map)
{
    const auto map = forestMap();
    OLAM_CHECK(map != nullptr);
    if (!map)
        return;
    ResourceState state;
    const std::size_t f = firstForage(*map, true);
    state.harvestForage(*map, f, 0.3f);
    state.fellTree(*map, TreeId::fromIndex(0));
    const auto bytes = state.serialize(*map);

    ResourceState loaded;
    OLAM_CHECK(!loaded.deserialize(*map, bytes).has_value());
    OLAM_CHECK(loaded.forageRemaining(*map, f) == state.forageRemaining(*map, f));
    OLAM_CHECK(loaded.treeFelled(*map, TreeId::fromIndex(0)));
    OLAM_CHECK(loaded.serialize(*map) == bytes);

    // Truncated data and data of another settlement are rejected and leave the state untouched.
    ResourceState fresh;
    OLAM_CHECK(fresh.deserialize(*map, std::span<const std::uint8_t>(bytes).first(bytes.size() - 2)).has_value());
    std::vector<std::uint8_t> otherSite = bytes;
    otherSite[16] ^= 0x01;
    OLAM_CHECK(fresh.deserialize(*map, otherSite).has_value());
    OLAM_CHECK(fresh.changedForageCount() == 0 && fresh.felledTreeCount() == 0);
}
