#include "world/WorldIO.h"

#include "core/files/FileSystem.h"
#include "world/WorldHash.h"
#include "world/WorldLayers.h"

#include <array>
#include <format>
#include <type_traits>
#include <utility>

namespace olam
{

    namespace
    {

        constexpr std::array<std::uint8_t, 8> kMagic = {'O', 'L', 'A', 'M', 'W', 'R', 'L', 'D'};

        template <typename T>
        struct IsStrongId : std::false_type
        {
        };

        template <typename Tag>
        struct IsStrongId<StrongId<Tag>> : std::true_type
        {
        };

        std::size_t entityCount(const World &world, RiverId) { return world.hydrology().rivers.size(); }
        std::size_t entityCount(const World &world, LakeId) { return world.hydrology().lakes.size(); }
        std::size_t entityCount(const World &world, DepositId) { return world.resources().deposits.size(); }
        std::size_t entityCount(const World &world, WatershedId) { return world.hydrology().watersheds.size(); }

        template <typename Id>
        bool isValidReference(const World &world, Id id)
        {
            return id.value <= entityCount(world, id);
        }

        template <typename T>
        bool isValidValue(const World &world, T value)
        {
            if constexpr (std::is_same_v<T, Direction8>)
                return value < Direction8::Count || value == kNoFlow;
            else if constexpr (std::is_enum_v<T>)
                return value < T::Count;
            else if constexpr (IsStrongId<T>::value)
                return isValidReference(world, value);
            else
                return true;
        }

        void writeCoord(ByteWriter &writer, WorldCoord coord)
        {
            writer.write(coord.x);
            writer.write(coord.y);
        }

        bool readCoord(ByteReader &reader, WorldCoord &coord)
        {
            return reader.read(coord.x) && reader.read(coord.y);
        }

        // Reads an element count, rejecting counts the remaining bytes cannot hold (bounds allocations).
        bool readCount(ByteReader &reader, std::size_t minBytesEach, std::uint32_t &count)
        {
            return reader.read(count) && static_cast<std::uint64_t>(count) * minBytesEach <= reader.remaining();
        }

        std::optional<std::string> readHydrologyEntities(ByteReader &reader, World &world)
        {
            HydrologyData &hydrology = world.hydrology();
            std::uint32_t riverCount = 0;
            if (!readCount(reader, 4, riverCount))
                return "truncated river list";
            hydrology.rivers.resize(riverCount);
            for (std::uint32_t r = 0; r < riverCount; ++r)
            {
                River &river = hydrology.rivers[r];
                std::uint32_t pathLength = 0;
                if (!reader.read(river.id) || river.id != RiverId::fromIndex(r) || !readCount(reader, 8, pathLength) ||
                    pathLength == 0)
                    return std::format("invalid river {}", r + 1);
                river.path.resize(pathLength);
                for (WorldCoord &tile : river.path)
                {
                    if (!readCoord(reader, tile) || !world.isValid(tile))
                        return std::format("invalid path of river {}", r + 1);
                }
                std::uint32_t tributaryCount = 0;
                if (!reader.read(river.mouthDischarge) || !reader.read(river.order) || river.order == 0 ||
                    !reader.read(river.endsIn) || river.endsIn >= RiverEnd::Count ||
                    !reader.read(river.flowsInto) || !reader.read(river.lake) || !readCount(reader, 4, tributaryCount))
                    return std::format("invalid river {}", r + 1);
                river.tributaries.resize(tributaryCount);
                for (RiverId &tributary : river.tributaries)
                {
                    if (!reader.read(tributary))
                        return std::format("invalid tributaries of river {}", r + 1);
                }
            }

            std::uint32_t lakeCount = 0;
            if (!readCount(reader, 4, lakeCount))
                return "truncated lake list";
            hydrology.lakes.resize(lakeCount);
            for (std::uint32_t l = 0; l < lakeCount; ++l)
            {
                Lake &lake = hydrology.lakes[l];
                std::uint32_t inflowCount = 0;
                if (!reader.read(lake.id) || lake.id != LakeId::fromIndex(l) || !reader.read(lake.surfaceElevation) ||
                    !reader.read(lake.tileCount) || !readCoord(reader, lake.outlet) || !world.isValid(lake.outlet) ||
                    !reader.read(lake.outflow) || !readCount(reader, 4, inflowCount))
                    return std::format("invalid lake {}", l + 1);
                lake.inflows.resize(inflowCount);
                for (RiverId &inflow : lake.inflows)
                {
                    if (!reader.read(inflow))
                        return std::format("invalid inflows of lake {}", l + 1);
                }
            }

            // References are checked once both lists are known.
            for (const River &river : hydrology.rivers)
            {
                bool valid = isValidReference(world, river.flowsInto) && isValidReference(world, river.lake);
                for (const RiverId tributary : river.tributaries)
                    valid = valid && isValidReference(world, tributary);
                if (!valid)
                    return std::format("river {} references a missing river or lake", river.id.value);
            }
            for (const Lake &lake : hydrology.lakes)
            {
                bool valid = isValidReference(world, lake.outflow);
                for (const RiverId inflow : lake.inflows)
                    valid = valid && isValidReference(world, inflow);
                if (!valid)
                    return std::format("lake {} references a missing river", lake.id.value);
            }

            std::uint32_t watershedCount = 0;
            if (!readCount(reader, 4, watershedCount))
                return "truncated watershed list";
            hydrology.watersheds.resize(watershedCount);
            for (std::uint32_t w = 0; w < watershedCount; ++w)
            {
                Watershed &watershed = hydrology.watersheds[w];
                std::uint32_t basinRivers = 0;
                std::uint32_t basinLakes = 0;
                if (!reader.read(watershed.id) || watershed.id != WatershedId::fromIndex(w) ||
                    !readCoord(reader, watershed.outlet) || !world.isValid(watershed.outlet) ||
                    !reader.read(watershed.tileCount) || !reader.read(watershed.outletDischarge) ||
                    !reader.read(watershed.mainRiver) || !isValidReference(world, watershed.mainRiver) ||
                    !readCount(reader, 4, basinRivers))
                    return std::format("invalid watershed {}", w + 1);
                watershed.rivers.resize(basinRivers);
                for (RiverId &river : watershed.rivers)
                {
                    if (!reader.read(river) || !isValidReference(world, river))
                        return std::format("invalid rivers of watershed {}", w + 1);
                }
                if (!readCount(reader, 4, basinLakes))
                    return std::format("invalid watershed {}", w + 1);
                watershed.lakes.resize(basinLakes);
                for (LakeId &lake : watershed.lakes)
                {
                    if (!reader.read(lake) || !isValidReference(world, lake))
                        return std::format("invalid lakes of watershed {}", w + 1);
                }
            }
            return std::nullopt;
        }

        std::optional<std::string> readResourceEntities(ByteReader &reader, World &world)
        {
            ResourceData &resources = world.resources();
            std::uint32_t depositCount = 0;
            if (!readCount(reader, 4, depositCount))
                return "truncated deposit list";
            resources.deposits.resize(depositCount);
            for (std::uint32_t d = 0; d < depositCount; ++d)
            {
                Deposit &deposit = resources.deposits[d];
                if (!reader.read(deposit.id) || deposit.id != DepositId::fromIndex(d) || !reader.read(deposit.mineral) ||
                    deposit.mineral >= MineralType::Count || !reader.read(deposit.origin) ||
                    deposit.origin >= DepositOrigin::Count || !readCoord(reader, deposit.center) ||
                    !world.isValid(deposit.center) || !reader.read(deposit.tileCount) || !reader.read(deposit.richness))
                    return std::format("invalid deposit {}", d + 1);
            }
            return std::nullopt;
        }

        WorldLoadResult fail(std::string message)
        {
            return {nullptr, std::move(message)};
        }

    } // namespace

    void writeHydrologyEntities(ByteWriter &writer, const HydrologyData &hydrology)
    {
        writer.write(static_cast<std::uint32_t>(hydrology.rivers.size()));
        for (const River &river : hydrology.rivers)
        {
            writer.write(river.id);
            writer.write(static_cast<std::uint32_t>(river.path.size()));
            for (const WorldCoord tile : river.path)
                writeCoord(writer, tile);
            writer.write(river.mouthDischarge);
            writer.write(river.order);
            writer.write(river.endsIn);
            writer.write(river.flowsInto);
            writer.write(river.lake);
            writer.write(static_cast<std::uint32_t>(river.tributaries.size()));
            for (const RiverId tributary : river.tributaries)
                writer.write(tributary);
        }
        writer.write(static_cast<std::uint32_t>(hydrology.lakes.size()));
        for (const Lake &lake : hydrology.lakes)
        {
            writer.write(lake.id);
            writer.write(lake.surfaceElevation);
            writer.write(lake.tileCount);
            writeCoord(writer, lake.outlet);
            writer.write(lake.outflow);
            writer.write(static_cast<std::uint32_t>(lake.inflows.size()));
            for (const RiverId inflow : lake.inflows)
                writer.write(inflow);
        }
        writer.write(static_cast<std::uint32_t>(hydrology.watersheds.size()));
        for (const Watershed &watershed : hydrology.watersheds)
        {
            writer.write(watershed.id);
            writeCoord(writer, watershed.outlet);
            writer.write(watershed.tileCount);
            writer.write(watershed.outletDischarge);
            writer.write(watershed.mainRiver);
            writer.write(static_cast<std::uint32_t>(watershed.rivers.size()));
            for (const RiverId river : watershed.rivers)
                writer.write(river);
            writer.write(static_cast<std::uint32_t>(watershed.lakes.size()));
            for (const LakeId lake : watershed.lakes)
                writer.write(lake);
        }
    }

    void writeResourceEntities(ByteWriter &writer, const ResourceData &resources)
    {
        writer.write(static_cast<std::uint32_t>(resources.deposits.size()));
        for (const Deposit &deposit : resources.deposits)
        {
            writer.write(deposit.id);
            writer.write(deposit.mineral);
            writer.write(deposit.origin);
            writeCoord(writer, deposit.center);
            writer.write(deposit.tileCount);
            writer.write(deposit.richness);
        }
    }

    std::vector<std::uint8_t> serializeWorld(const World &world)
    {
        ByteWriter writer;
        std::uint32_t layerCount = 0;
        std::size_t layerBytes = 0;
        for (const WorldLayerDescriptor &descriptor : worldLayerDescriptors())
        {
            visitLayer(world, descriptor.id, [&](const auto &layer)
                       {
                           if (layer.empty())
                               return;
                           ++layerCount;
                           layerBytes += layer.size() * sizeof(layer[0]); });
        }
        writer.reserve(layerBytes + 4096);

        for (const std::uint8_t byte : kMagic)
            writer.write(byte);
        writer.write(kWorldFileVersion);
        visitWorldConfig(world.config(), [&](const auto &value)
                         { writer.write(value); });
        writer.write(world.seed());

        writer.write(layerCount);
        for (const WorldLayerDescriptor &descriptor : worldLayerDescriptors())
        {
            visitLayer(world, descriptor.id, [&](const auto &layer)
                       {
                           if (layer.empty())
                               return;
                           writer.write(descriptor.id);
                           writer.write(static_cast<std::int32_t>(layer.width()));
                           writer.write(static_cast<std::int32_t>(layer.height()));
                           for (const auto &value : layer.values())
                               writer.write(value); });
        }

        writeHydrologyEntities(writer, world.hydrology());
        writeResourceEntities(writer, world.resources());
        writer.write(hashWorld(world).combined);
        return writer.takeBytes();
    }

    WorldLoadResult deserializeWorld(std::span<const std::uint8_t> bytes)
    {
        ByteReader reader(bytes);
        for (const std::uint8_t expected : kMagic)
        {
            std::uint8_t byte = 0;
            if (!reader.read(byte) || byte != expected)
                return fail("not an Olam world file");
        }
        std::uint32_t version = 0;
        if (!reader.read(version))
            return fail("truncated header");
        if (version != kWorldFileVersion)
            return fail(std::format("unsupported world file version {} (expected {})", version, kWorldFileVersion));

        WorldConfig config;
        visitWorldConfig(config, [&](auto &value)
                         { reader.read(value); });
        std::uint64_t seed = 0;
        if (!reader.read(seed))
            return fail("truncated config");
        if (auto error = validateWorldConfig(config))
            return fail("invalid config: " + *error);

        auto world = std::make_unique<World>(config, seed);

        std::uint32_t layerCount = 0;
        if (!reader.read(layerCount) || layerCount > static_cast<std::uint32_t>(LayerId::Count))
            return fail("invalid layer count");
        bool seen[static_cast<std::size_t>(LayerId::Count)] = {};
        for (std::uint32_t n = 0; n < layerCount; ++n)
        {
            LayerId id{};
            std::int32_t width = 0;
            std::int32_t height = 0;
            if (!reader.read(id) || id >= LayerId::Count || seen[static_cast<std::size_t>(id)])
                return fail("invalid or duplicate layer id");
            seen[static_cast<std::size_t>(id)] = true;
            if (!reader.read(width) || !reader.read(height) || width != config.width || height != config.height)
                return fail("layer size does not match the world size");
            bool ok = true;
            visitLayer(*world, id, [&](auto &layer)
                       {
                           if (static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * sizeof(layer[0]) >
                               reader.remaining())
                           {
                               ok = false;
                               return;
                           }
                           layer.resize(width, height);
                           for (auto &value : layer.values())
                               reader.read(value);
                           ok = !reader.failed(); });
            if (!ok)
                return fail("truncated layer data");
        }

        if (auto error = readHydrologyEntities(reader, *world))
            return fail(*error);
        if (auto error = readResourceEntities(reader, *world))
            return fail(*error);

        // Enum and id layers are validated after the entities they reference are known.
        for (const WorldLayerDescriptor &descriptor : worldLayerDescriptors())
        {
            bool valid = true;
            visitLayer(*world, descriptor.id, [&](const auto &layer)
                       {
                           for (const auto &value : layer.values())
                           {
                               if (!isValidValue(*world, value))
                               {
                                   valid = false;
                                   return;
                               }
                           } });
            if (!valid)
                return fail(std::format("layer '{}' holds out-of-range values", descriptor.name));
        }

        std::uint64_t storedHash = 0;
        if (!reader.read(storedHash))
            return fail("truncated world hash");
        if (reader.remaining() != 0)
            return fail("unexpected data after the world hash");
        if (storedHash != hashWorld(*world).combined)
            return fail("world hash mismatch (file is corrupt or was modified)");
        return {std::move(world), {}};
    }

    std::optional<std::string> saveWorld(const World &world, const std::filesystem::path &path)
    {
        const std::vector<std::uint8_t> bytes = serializeWorld(world);
        if (!files::writeBinaryFile(path, bytes))
            return std::format("could not write '{}'", path.string());
        return std::nullopt;
    }

    WorldLoadResult loadWorld(const std::filesystem::path &path)
    {
        const auto bytes = files::readBinaryFile(path);
        if (!bytes)
            return fail(std::format("could not read '{}'", path.string()));
        WorldLoadResult result = deserializeWorld(*bytes);
        if (!result.world)
            result.error = std::format("'{}': {}", path.string(), result.error);
        return result;
    }

} // namespace olam
