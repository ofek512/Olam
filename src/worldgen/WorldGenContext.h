#pragma once

#include "core/containers/Layer.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace olam
{

    class World;
    class WorldGenerationPass;

    // State shared by passes during one generation run. Working layers are discarded with the context.
    class WorldGenContext
    {
    public:
        explicit WorldGenContext(World &world);

        World &world() { return m_world; }
        const World &world() const { return m_world; }

        std::uint64_t seedFor(const WorldGenerationPass &pass) const;

        // Creates a zero-filled float layer sized to the world; the name must not already exist.
        Layer<float> &createWorkingLayer(std::string_view name);
        Layer<float> *findWorkingLayer(std::string_view name);
        const Layer<float> *findWorkingLayer(std::string_view name) const;
        void releaseWorkingLayer(std::string_view name);
        std::size_t workingLayerCount() const { return m_workingLayers.size(); }

    private:
        struct WorkingLayer
        {
            std::string name;
            Layer<float> layer;
        };

        World &m_world;
        // unique_ptr keeps references returned by createWorkingLayer stable.
        std::vector<std::unique_ptr<WorkingLayer>> m_workingLayers;
    };

} // namespace olam
