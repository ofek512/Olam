#include "settlement/ResourceState.h"

#include "core/serialization/ByteReader.h"
#include "core/serialization/ByteWriter.h"
#include "settlement/SettlementMap.h"

#include <algorithm>
#include <cmath>

namespace olam
{

    namespace
    {

        constexpr std::uint32_t kMagic = 0x53524C4F; // "OLRS"
        // Version 1 had no cleared understory; it still loads.
        constexpr std::uint32_t kVersion = 2;

        std::uint32_t tileKey(std::uint16_t x, std::uint16_t y) { return (static_cast<std::uint32_t>(y) << 16) | x; }

    } // namespace

    float treeWoodKg(TreeKind kind, std::uint8_t growth)
    {
        // Rough fresh-wood mass of a mature tree per category.
        constexpr float kMatureKg[static_cast<std::size_t>(TreeKind::Count)] = {1200.0f, 900.0f, 200.0f, 600.0f, 500.0f};
        const float size = static_cast<float>(growth) / 255.0f;
        return std::max(10.0f, kMatureKg[static_cast<std::size_t>(kind)] * size * size);
    }

    float ResourceState::forageRemaining(const SettlementMap &map, std::size_t f) const
    {
        const ForageData &forage = map.forage();
        const auto it = m_forageTaken.find(tileKey(forage.x[f], forage.y[f]));
        const float taken = it != m_forageTaken.end() ? it->second : 0.0f;
        return std::max(0.0f, forage.kilograms(f) - taken);
    }

    float ResourceState::harvestForage(const SettlementMap &map, std::size_t f, float kg)
    {
        const float taken = std::min(std::max(0.0f, kg), forageRemaining(map, f));
        if (taken <= 0.0f)
            return 0.0f;
        const ForageData &forage = map.forage();
        m_forageTaken[tileKey(forage.x[f], forage.y[f])] += taken;
        ++m_revision;
        return taken;
    }

    bool ResourceState::treeFelled(const SettlementMap &map, TreeId tree) const
    {
        const TreeData &trees = map.trees();
        return m_felledTrees.contains(tileKey(trees.x[tree.index()], trees.y[tree.index()]));
    }

    float ResourceState::fellTree(const SettlementMap &map, TreeId tree)
    {
        const TreeData &trees = map.trees();
        const std::size_t t = tree.index();
        if (!m_felledTrees.insert(tileKey(trees.x[t], trees.y[t])).second)
            return 0.0f;
        ++m_revision;
        return treeWoodKg(trees.kind[t], trees.growth[t]);
    }

    bool ResourceState::understoryCleared(const SettlementMap &map, std::size_t u) const
    {
        const UnderstoryData &understory = map.understory();
        return m_clearedUnderstory.contains(tileKey(understory.x[u], understory.y[u]));
    }

    bool ResourceState::clearUnderstory(const SettlementMap &map, std::size_t u)
    {
        const UnderstoryData &understory = map.understory();
        if (!m_clearedUnderstory.insert(tileKey(understory.x[u], understory.y[u])).second)
            return false;
        ++m_revision;
        return true;
    }

    void ResourceState::regrow(const SettlementMap &map, float years)
    {
        if (years <= 0.0f || m_forageTaken.empty())
            return;
        for (auto it = m_forageTaken.begin(); it != m_forageTaken.end();)
        {
            const auto f = map.forageAt({static_cast<std::int32_t>(it->first & 0xFFFFu), static_cast<std::int32_t>(it->first >> 16)});
            if (!f)
            {
                it = m_forageTaken.erase(it);
                continue;
            }
            const ForageData &forage = map.forage();
            it->second -= forageInfo(forage.kind[*f]).regrowthPerYear * forage.kilograms(*f) * years;
            it = it->second <= 1e-3f ? m_forageTaken.erase(it) : std::next(it);
        }
        ++m_revision;
    }

    std::vector<std::uint8_t> ResourceState::serialize(const SettlementMap &map) const
    {
        ByteWriter writer;
        writer.write(kMagic);
        writer.write(kVersion);
        writer.write(map.worldSeed());
        writer.write(map.site().x);
        writer.write(map.site().y);
        writer.write(static_cast<std::uint32_t>(m_forageTaken.size()));
        for (const auto &[key, taken] : m_forageTaken)
        {
            writer.write(key);
            writer.write(taken);
        }
        writer.write(static_cast<std::uint32_t>(m_felledTrees.size()));
        for (const std::uint32_t key : m_felledTrees)
            writer.write(key);
        writer.write(static_cast<std::uint32_t>(m_clearedUnderstory.size()));
        for (const std::uint32_t key : m_clearedUnderstory)
            writer.write(key);
        return writer.takeBytes();
    }

    std::optional<std::string> ResourceState::deserialize(const SettlementMap &map, std::span<const std::uint8_t> bytes)
    {
        ByteReader reader(bytes);
        std::uint32_t magic = 0;
        std::uint32_t version = 0;
        std::uint64_t worldSeed = 0;
        WorldCoord site;
        reader.read(magic);
        reader.read(version);
        reader.read(worldSeed);
        reader.read(site.x);
        reader.read(site.y);
        if (reader.failed() || magic != kMagic)
            return "not a resource state";
        if (version < 1 || version > kVersion)
            return "unsupported resource state version " + std::to_string(version);
        if (worldSeed != map.worldSeed() || !(site == map.site()))
            return "resource state belongs to another settlement";

        std::map<std::uint32_t, float> forage;
        std::set<std::uint32_t> felled;
        std::set<std::uint32_t> cleared;
        std::uint32_t count = 0;
        reader.read(count);
        for (std::uint32_t i = 0; i < count && !reader.failed(); ++i)
        {
            std::uint32_t key = 0;
            float taken = 0.0f;
            reader.read(key);
            reader.read(taken);
            if (!(taken >= 0.0f) || !std::isfinite(taken))
                return "corrupt forage entry";
            forage[key] = taken;
        }
        reader.read(count);
        for (std::uint32_t i = 0; i < count && !reader.failed(); ++i)
        {
            std::uint32_t key = 0;
            reader.read(key);
            felled.insert(key);
        }
        if (version >= 2)
        {
            reader.read(count);
            for (std::uint32_t i = 0; i < count && !reader.failed(); ++i)
            {
                std::uint32_t key = 0;
                reader.read(key);
                cleared.insert(key);
            }
        }
        if (reader.failed())
            return "truncated resource state";
        m_forageTaken = std::move(forage);
        m_felledTrees = std::move(felled);
        m_clearedUnderstory = std::move(cleared);
        ++m_revision;
        return std::nullopt;
    }

} // namespace olam
