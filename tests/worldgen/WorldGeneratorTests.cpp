#include "TestFramework.h"

#include "core/hash/LayerHash.h"
#include "core/random/Pcg32.h"
#include "core/random/Seed.h"
#include "worldgen/WorldGenContext.h"
#include "worldgen/WorldGenerator.h"

#include <string>
#include <vector>

using namespace olam;

namespace
{
    WorldConfig smallConfig()
    {
        WorldConfig config;
        config.width = 32;
        config.height = 16;
        return config;
    }

    struct PassLog
    {
        std::vector<std::string> order;
        std::vector<std::uint64_t> seeds;
    };

    class RecordingPass : public WorldGenerationPass
    {
    public:
        RecordingPass(std::string name, std::uint64_t id, PassLog &log) : m_name(std::move(name)), m_id(id), m_log(log) {}

        std::string_view name() const override { return m_name; }
        std::uint64_t seedId() const override { return m_id; }
        void run(WorldGenContext &context) override
        {
            m_log.order.push_back(m_name);
            m_log.seeds.push_back(context.seedFor(*this));
        }

    private:
        std::string m_name;
        std::uint64_t m_id;
        PassLog &m_log;
    };

    class FailingPass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "failing"; }
        std::uint64_t seedId() const override { return olam::seedId("FAIL"); }
        std::optional<std::string> validatePreconditions(const WorldGenContext &) const override
        {
            return std::string("missing input layer");
        }
        void run(WorldGenContext &) override { ran = true; }

        bool ran = false;
    };

    // Fills a working layer from its seed and reports the layer hash.
    class NoisePass : public WorldGenerationPass
    {
    public:
        explicit NoisePass(std::uint64_t &hashOut) : m_hashOut(hashOut) {}

        std::string_view name() const override { return "noise"; }
        std::uint64_t seedId() const override { return olam::seedId("NOISE"); }
        void run(WorldGenContext &context) override
        {
            Layer<float> &noise = context.createWorkingLayer("noise");
            Pcg32 rng(context.seedFor(*this));
            for (float &value : noise.values())
                value = rng.nextFloat01();

            Layer<std::uint32_t> quantized(noise.width(), noise.height());
            for (std::size_t i = 0; i < noise.size(); ++i)
                quantized[i] = static_cast<std::uint32_t>(noise[i] * 16777216.0f);
            m_hashOut = hashLayer(quantized);

            context.releaseWorkingLayer("noise");
            m_layerReleased = context.workingLayerCount() == 0;
        }

        bool layerReleased() const { return m_layerReleased; }

    private:
        std::uint64_t &m_hashOut;
        bool m_layerReleased = false;
    };
} // namespace

OLAM_TEST(worldgen_empty_pipeline_produces_world)
{
    WorldGenerator generator;
    const WorldGenResult result = generator.generate(smallConfig(), 777);
    OLAM_CHECK(result.ok());
    OLAM_CHECK(result.error.empty());
    OLAM_CHECK(result.world && result.world->seed() == 777);
    OLAM_CHECK(result.world && result.world->width() == 32 && result.world->height() == 16);
}

OLAM_TEST(worldgen_rejects_invalid_config)
{
    WorldConfig config = smallConfig();
    config.width = 1000;
    WorldGenerator generator;
    const WorldGenResult result = generator.generate(config, 1);
    OLAM_CHECK(!result.ok());
    OLAM_CHECK(!result.error.empty());
}

OLAM_TEST(worldgen_runs_passes_in_order_with_derived_seeds)
{
    PassLog log;
    WorldGenerator generator;
    generator.addPass(std::make_unique<RecordingPass>("first", seedId("FIRST"), log));
    generator.addPass(std::make_unique<RecordingPass>("second", seedId("SECOND"), log));

    const WorldGenResult result = generator.generate(smallConfig(), 4242);
    OLAM_CHECK(result.ok());
    OLAM_CHECK((log.order == std::vector<std::string>{"first", "second"}));
    OLAM_CHECK(log.seeds.size() == 2);
    OLAM_CHECK(log.seeds.size() == 2 && log.seeds[0] == deriveSeed(4242, seedId("FIRST")));
    OLAM_CHECK(log.seeds.size() == 2 && log.seeds[1] == deriveSeed(4242, seedId("SECOND")));
    OLAM_CHECK(result.timings.size() == 2);
}

OLAM_TEST(worldgen_stops_on_failed_preconditions)
{
    PassLog log;
    WorldGenerator generator;
    auto failing = std::make_unique<FailingPass>();
    FailingPass *failingPtr = failing.get();
    generator.addPass(std::move(failing));
    generator.addPass(std::make_unique<RecordingPass>("after", seedId("AFTER"), log));

    const WorldGenResult result = generator.generate(smallConfig(), 1);
    OLAM_CHECK(!result.ok());
    OLAM_CHECK(result.error.find("failing") != std::string::npos);
    OLAM_CHECK(!failingPtr->ran);
    OLAM_CHECK(log.order.empty());
}

OLAM_TEST(worldgen_same_seed_same_result)
{
    std::uint64_t hashA = 0;
    std::uint64_t hashB = 0;
    std::uint64_t hashC = 0;

    WorldGenerator a;
    auto noiseA = std::make_unique<NoisePass>(hashA);
    const NoisePass *noiseAPtr = noiseA.get();
    a.addPass(std::move(noiseA));
    WorldGenerator b;
    b.addPass(std::make_unique<NoisePass>(hashB));
    WorldGenerator c;
    c.addPass(std::make_unique<NoisePass>(hashC));

    OLAM_CHECK(a.generate(smallConfig(), 99).ok());
    OLAM_CHECK(b.generate(smallConfig(), 99).ok());
    OLAM_CHECK(c.generate(smallConfig(), 100).ok());

    OLAM_CHECK(hashA == hashB);
    OLAM_CHECK(hashA != hashC);
    OLAM_CHECK(noiseAPtr->layerReleased());
}

OLAM_TEST(worldgen_context_working_layers)
{
    World world(smallConfig(), 1);
    WorldGenContext context(world);

    Layer<float> &first = context.createWorkingLayer("first");
    OLAM_CHECK(first.width() == 32 && first.height() == 16);
    first.at(3, 4) = 2.5f;

    context.createWorkingLayer("second");
    OLAM_CHECK(context.workingLayerCount() == 2);
    OLAM_CHECK(context.findWorkingLayer("first") == &first);
    OLAM_CHECK(context.findWorkingLayer("first")->at(3, 4) == 2.5f);
    OLAM_CHECK(context.findWorkingLayer("missing") == nullptr);

    context.releaseWorkingLayer("first");
    OLAM_CHECK(context.findWorkingLayer("first") == nullptr);
    OLAM_CHECK(context.workingLayerCount() == 1);
}
