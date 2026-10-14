#include "settlement/RoomFunctions.h"

#include "core/debug/Assert.h"
#include "settlement/ConstructionState.h"

#include <algorithm>
#include <set>

namespace olam
{

    namespace
    {

        constexpr std::uint32_t kBeds = furnitureBit(FurnitureKind::Bed) | furnitureBit(FurnitureKind::DoubleBed);
        constexpr std::uint32_t kSleeping = kBeds | furnitureBit(FurnitureKind::Bedroll);
        constexpr std::uint32_t kStorage = furnitureBit(FurnitureKind::StorageChest) | furnitureBit(FurnitureKind::Shelf);

        // Rooms in the spirit of RimWorld (furniture makes the room), Songs of Syx (rooms as workplaces) and
        // Civilization (each kind of building yields one service).
        constexpr std::array<RoomFunctionInfo, kRoomFunctionCount> kCatalogue = {{
            {RoomFunction::Unassigned, "Unassigned", "An enclosed room without a purpose yet", RoomService::None, HousingKind::None, {}, 0,
             0, 0, "Shelter"},
            {RoomFunction::Bedroom, "Bedroom", "Private sleeping room for one person or a couple", RoomService::Housing,
             HousingKind::Private, {{{kBeds, 1, "a bed or double bed"}, {}}}, kBeds, 2, 90, "House"},
            {RoomFunction::Dormitory, "Dormitory", "Shared sleeping hall: many sleepers, little privacy", RoomService::Housing,
             HousingKind::Communal, {{{kSleeping, 1, "a bedroll or bed"}, {}}}, kSleeping, 0, 40, "Dormitory"},
            {RoomFunction::Storage, "Storeroom", "Keeps goods dry and safe", RoomService::Storage, HousingKind::None,
             {{{kStorage, 1, "a chest or shelf"}, {}}}, kStorage, 0, 0, "Storehouse"},
            {RoomFunction::Kitchen, "Kitchen", "Cooks meals over a hearth", RoomService::Food, HousingKind::None,
             {{{furnitureBit(FurnitureKind::CookingHearth), 1, "a hearth"}, {}}}, furnitureBit(FurnitureKind::CookingHearth), 0, 0,
             "Cookhouse"},
            {RoomFunction::Workshop, "Workshop", "Artisans make tools and goods at workbenches", RoomService::Production,
             HousingKind::None, {{{furnitureBit(FurnitureKind::Workbench), 1, "a workbench"}, {}}},
             furnitureBit(FurnitureKind::Workbench), 0, 0, "Workshop"},
            {RoomFunction::DiningHall, "Dining hall", "Tables and seats for meals and gatherings", RoomService::Amenity,
             HousingKind::None,
             {{{furnitureBit(FurnitureKind::Table), 1, "a table"}, {furnitureBit(FurnitureKind::Chair), 1, "a stool"}}},
             furnitureBit(FurnitureKind::Table) | furnitureBit(FurnitureKind::Chair), 0, 0, "Hall"},
            {RoomFunction::Shrine, "Shrine", "A place of worship around an altar", RoomService::Faith, HousingKind::None,
             {{{furnitureBit(FurnitureKind::Altar), 1, "an altar"}, {}}}, furnitureBit(FurnitureKind::Altar), 0, 0, "Shrine"},
            {RoomFunction::Infirmary, "Infirmary", "Beds where the sick and injured are tended", RoomService::Health, HousingKind::None,
             {{{furnitureBit(FurnitureKind::Bed), 1, "a bed"}, {}}}, furnitureBit(FurnitureKind::Bed), 0, 0, "Infirmary"},
        }};

        // Worshippers per tile of a shrine (2 m2 each on 4 m2 tiles).
        constexpr std::uint32_t kWorshippersPerTile = 2;
        // Seats a table can serve.
        constexpr std::uint32_t kSeatsPerTable = 4;
        // Integrity of a wall that counts as condition 100 (rough stone).
        constexpr std::uint32_t kSturdyWall = 600;

        std::uint32_t countOf(const std::array<std::uint32_t, kFurnitureCount> &counts, std::uint32_t kinds)
        {
            std::uint32_t n = 0;
            for (std::size_t k = 0; k < kFurnitureCount; ++k)
            {
                if (kinds & (1u << k))
                    n += counts[k];
            }
            return n;
        }

        bool meets(const RoomFunctionInfo &info, const std::array<std::uint32_t, kFurnitureCount> &counts,
                   std::vector<std::string_view> *missing)
        {
            bool ok = true;
            for (const FurnitureRequirement &requirement : info.required)
            {
                if (requirement.count == 0 || countOf(counts, requirement.kinds) >= requirement.count)
                    continue;
                ok = false;
                if (missing)
                    missing->push_back(requirement.what);
            }
            return ok;
        }

    } // namespace

    std::string_view toString(RoomService service)
    {
        switch (service)
        {
        case RoomService::None:
            return "none";
        case RoomService::Housing:
            return "housing";
        case RoomService::Storage:
            return "storage";
        case RoomService::Food:
            return "food";
        case RoomService::Production:
            return "production";
        case RoomService::Amenity:
            return "amenity";
        case RoomService::Faith:
            return "faith";
        case RoomService::Health:
            return "health";
        case RoomService::Count:
            break;
        }
        return "?";
    }

    const RoomFunctionInfo &roomFunctionInfo(RoomFunction function)
    {
        OLAM_ASSERT(function < RoomFunction::Count);
        return kCatalogue[static_cast<std::size_t>(function)];
    }

    std::span<const RoomFunctionInfo> roomFunctionCatalogue() { return kCatalogue; }

    std::string_view toString(RoomFunction function)
    {
        return function < RoomFunction::Count ? roomFunctionInfo(function).name : std::string_view{"?"};
    }

    std::vector<RoomFunction> functionsUsing(FurnitureKind kind)
    {
        std::vector<RoomFunction> functions;
        for (const RoomFunctionInfo &info : kCatalogue)
        {
            bool uses = (info.serving & furnitureBit(kind)) != 0;
            for (const FurnitureRequirement &requirement : info.required)
                uses = uses || (requirement.kinds & furnitureBit(kind)) != 0;
            if (uses)
                functions.push_back(info.function);
        }
        return functions;
    }

    RoomStatus evaluateRoom(const ConstructionState &construction, RoomId id)
    {
        RoomStatus status;
        const Room *room = construction.rooms().room(id);
        if (!room)
            return status;
        status.function = room->function;
        status.tiles = static_cast<std::uint32_t>(room->tiles.size());
        status.roofComplete = room->roofComplete();

        // Furniture lies inside one room: walls never cross a footprint.
        std::array<std::uint32_t, kFurnitureCount> counts{};
        std::set<std::uint32_t> seen;
        for (const LocalCoord tile : room->tiles)
        {
            for (const StructureId piece : construction.furnitureOnTile(tile))
            {
                if (!seen.insert(piece.value).second)
                    continue;
                const Structure &s = *construction.structure(piece);
                if (s.stage != ConstructionStage::Completed)
                {
                    ++status.unfinishedFurniture;
                    continue;
                }
                ++counts[static_cast<std::size_t>(s.furniture)];
                ++status.furniture;
                status.furnishing += furnitureInfo(s.furniture).comfort;
            }
        }

        for (const RoomFunctionInfo &info : kCatalogue)
        {
            if (info.function != RoomFunction::Unassigned && meets(info, counts, nullptr))
            {
                status.suggested = info.function;
                break;
            }
        }

        const RoomFunctionInfo &info = roomFunctionInfo(room->function);
        status.service = info.service;
        status.functional = room->function != RoomFunction::Unassigned && meets(info, counts, &status.missing);
        if (status.functional)
        {
            for (std::size_t k = 0; k < kFurnitureCount; ++k)
            {
                if ((info.serving & (1u << k)) == 0)
                    continue;
                const FurnitureInfo &piece = furnitureInfo(static_cast<FurnitureKind>(k));
                status.sleepers += piece.sleepers * counts[k];
                status.storageKg += piece.storageKg * counts[k];
                status.workplaces += piece.workplaces * counts[k];
                status.seats += piece.seats * counts[k];
            }
            if (info.maxSleepers > 0 && status.sleepers > info.maxSleepers)
            {
                status.crowding = status.sleepers - info.maxSleepers;
                status.sleepers = info.maxSleepers;
            }
            status.seats = std::min(status.seats, counts[static_cast<std::size_t>(FurnitureKind::Table)] * kSeatsPerTable);
            switch (info.service)
            {
            case RoomService::Housing:
            case RoomService::Health:
                status.serviceCapacity = status.sleepers;
                break;
            case RoomService::Storage:
                status.serviceCapacity = status.storageKg;
                break;
            case RoomService::Food:
            case RoomService::Production:
                status.serviceCapacity = status.workplaces;
                break;
            case RoomService::Amenity:
                status.serviceCapacity = status.seats;
                break;
            case RoomService::Faith:
                status.serviceCapacity = status.tiles * kWorshippersPerTile;
                break;
            case RoomService::None:
            case RoomService::Count:
                break;
            }
        }

        if (info.basePrivacy > 0)
        {
            const int extra = static_cast<int>(std::max<std::uint32_t>(status.sleepers, 1)) - 1;
            status.privacy = static_cast<std::uint8_t>(std::max(0, info.basePrivacy - 10 * extra));
        }

        // Condition: mean integrity of the walls around the room.
        std::uint32_t integrity = 0;
        std::uint32_t walls = 0;
        for (const LocalCoord tile : room->tiles)
        {
            for (const LocalEdge edge : tileEdges(tile))
            {
                const Structure *s = construction.structure(construction.edgeAt(edge));
                if (!s || s->kind != StructureKind::Wall || s->stage != ConstructionStage::Completed)
                    continue;
                const auto [a, b] = edgeTiles(edge);
                if (construction.rooms().roomAt(a) == id && construction.rooms().roomAt(b) == id)
                    continue;
                integrity += structureProperties(s->kind, s->material).maxIntegrity;
                ++walls;
            }
        }
        if (walls > 0)
            status.condition = static_cast<std::uint8_t>(std::min<std::uint32_t>(100, integrity / walls * 100 / kSturdyWall));

        // Comfort potential: space and furnishing per user, privacy and a finished roof.
        const std::uint32_t users = std::max<std::uint32_t>(status.sleepers, 1);
        const std::uint32_t space = std::min<std::uint32_t>(30, status.tiles * 4 * 3 / users);
        const std::uint32_t furnishing = std::min<std::uint32_t>(30, status.furnishing * 10 / users);
        const std::uint32_t comfort = space + furnishing + status.privacy * 3u / 10u + (status.roofComplete ? 13u : 0u);
        status.comfort = static_cast<std::uint8_t>(std::min<std::uint32_t>(100, comfort));
        return status;
    }

    BuildingSummary summarizeBuilding(const ConstructionState &construction, BuildingId id)
    {
        BuildingSummary summary;
        summary.name = roomFunctionInfo(RoomFunction::Unassigned).buildingName;
        const Building *building = construction.rooms().building(id);
        if (!building)
            return summary;
        std::array<std::uint32_t, kRoomFunctionCount> tilesByFunction{};
        for (const RoomId room : building->rooms)
        {
            const RoomStatus status = evaluateRoom(construction, room);
            ++summary.rooms;
            summary.tiles += status.tiles;
            if (status.function != RoomFunction::Unassigned)
                tilesByFunction[static_cast<std::size_t>(status.function)] += status.tiles;
            if (!status.functional)
                continue;
            const HousingKind housing = roomFunctionInfo(status.function).housing;
            if (housing == HousingKind::Private)
                summary.privateSleepers += status.sleepers;
            else if (housing == HousingKind::Communal)
                summary.communalSleepers += status.sleepers;
        }
        if (summary.privateSleepers > 0)
            summary.main = RoomFunction::Bedroom;
        else if (summary.communalSleepers > 0)
            summary.main = RoomFunction::Dormitory;
        else
        {
            std::uint32_t most = 0;
            for (std::size_t f = 0; f < kRoomFunctionCount; ++f)
            {
                if (tilesByFunction[f] > most)
                {
                    most = tilesByFunction[f];
                    summary.main = static_cast<RoomFunction>(f);
                }
            }
        }
        summary.name = roomFunctionInfo(summary.main).buildingName;
        return summary;
    }

} // namespace olam
