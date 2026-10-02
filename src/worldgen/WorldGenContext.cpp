#include "worldgen/WorldGenContext.h"

#include "core/debug/Assert.h"
#include "core/random/Seed.h"
#include "world/World.h"
#include "worldgen/WorldGenerationPass.h"

#include <algorithm>

namespace olam
{

    WorldGenContext::WorldGenContext(World &world)
        : m_world(world)
    {
    }

    std::uint64_t WorldGenContext::seedFor(const WorldGenerationPass &pass) const
    {
        return deriveSeed(m_world.seed(), pass.seedId());
    }

    Layer<float> &WorldGenContext::createWorkingLayer(std::string_view name)
    {
        OLAM_ASSERT(findWorkingLayer(name) == nullptr);
        auto entry = std::make_unique<WorkingLayer>();
        entry->name = std::string(name);
        entry->layer.resize(m_world.width(), m_world.height(), 0.0f);
        m_workingLayers.push_back(std::move(entry));
        return m_workingLayers.back()->layer;
    }

    Layer<float> *WorldGenContext::findWorkingLayer(std::string_view name)
    {
        for (const auto &entry : m_workingLayers)
        {
            if (entry->name == name)
                return &entry->layer;
        }
        return nullptr;
    }

    const Layer<float> *WorldGenContext::findWorkingLayer(std::string_view name) const
    {
        for (const auto &entry : m_workingLayers)
        {
            if (entry->name == name)
                return &entry->layer;
        }
        return nullptr;
    }

    void WorldGenContext::releaseWorkingLayer(std::string_view name)
    {
        std::erase_if(m_workingLayers, [name](const auto &entry)
                      { return entry->name == name; });
    }

} // namespace olam
