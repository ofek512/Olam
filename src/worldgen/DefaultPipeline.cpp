#include "worldgen/DefaultPipeline.h"

#include "worldgen/WorldGenerator.h"
#include "worldgen/passes/ElevationPass.h"
#include "worldgen/passes/TectonicsPass.h"

#include <memory>

namespace olam
{

    void addDefaultPasses(WorldGenerator &generator)
    {
        generator.addPass(std::make_unique<TectonicsPass>());
        generator.addPass(std::make_unique<ElevationPass>());
    }

} // namespace olam
