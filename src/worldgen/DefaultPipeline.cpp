#include "worldgen/DefaultPipeline.h"

#include "worldgen/WorldGenerator.h"
#include "worldgen/passes/ElevationPass.h"
#include "worldgen/passes/HydrologyPass.h"
#include "worldgen/passes/OceanPass.h"
#include "worldgen/passes/RainfallPass.h"
#include "worldgen/passes/SoilPass.h"
#include "worldgen/passes/TectonicsPass.h"
#include "worldgen/passes/TemperaturePass.h"

#include <memory>

namespace olam
{

    void addDefaultPasses(WorldGenerator &generator)
    {
        generator.addPass(std::make_unique<TectonicsPass>());
        generator.addPass(std::make_unique<ElevationPass>());
        generator.addPass(std::make_unique<OceanPass>());
        generator.addPass(std::make_unique<TemperaturePass>());
        generator.addPass(std::make_unique<RainfallPass>());
        generator.addPass(std::make_unique<HydrologyPass>());
        generator.addPass(std::make_unique<SoilPass>());
    }

} // namespace olam
