#pragma once

#include "core/types/StrongId.h"
#include "settlement/Furniture.h"
#include "settlement/Goods.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace olam
{

    class ConstructionState;

    struct RoomTag;
    using RoomId = StrongId<RoomTag>;
    struct BuildingTag;
    using BuildingId = StrongId<BuildingTag>;

    // What the player designates a room for (Phase 4B.4). The designation is the player's: furniture never
    // changes it, it only decides whether the room works. Persisted: append only, never reorder.
    enum class RoomFunction : std::uint8_t
    {
        Unassigned,
        Bedroom,
        Dormitory,
        Storage,
        Kitchen,
        Workshop,
        DiningHall,
        Shrine,
        Infirmary,
        Count,
    };

    constexpr std::size_t kRoomFunctionCount = static_cast<std::size_t>(RoomFunction::Count);

    // What a working room provides to the settlement, like a Civilization building's yield. Only reported for
    // now; consumers arrive with the population (Phase 4C and later).
    enum class RoomService : std::uint8_t
    {
        None,
        // Sleeping places.
        Housing,
        // Kilograms of storage.
        Storage,
        // Cooking places.
        Food,
        // Work places for artisans.
        Production,
        // Seats for meals and gatherings.
        Amenity,
        // Worshippers.
        Faith,
        // Patients.
        Health,
        Count,
    };

    std::string_view toString(RoomService service);

    enum class HousingKind : std::uint8_t
    {
        None,
        // Low density, privacy, room for furniture.
        Private,
        // Many sleepers for little area and material.
        Communal,
    };

    // One requirement: at least `count` pieces among the kinds in `kinds` (a bit per FurnitureKind).
    struct FurnitureRequirement
    {
        std::uint32_t kinds = 0;
        std::uint8_t count = 0;
        // E.g. "a bed or double bed".
        std::string_view what;
    };

    constexpr std::uint32_t furnitureBit(FurnitureKind kind) { return 1u << static_cast<std::uint32_t>(kind); }

    struct RoomFunctionInfo
    {
        RoomFunction function;
        std::string_view name;
        std::string_view description;
        RoomService service;
        HousingKind housing;
        std::array<FurnitureRequirement, 2> required;
        // Furniture that counts towards the service (sleepers, storage, work places, seats).
        std::uint32_t serving;
        // Most sleepers the function allows (0: no limit); more are crowding.
        std::uint8_t maxSleepers;
        // Privacy of a single sleeper (0..100); every extra sleeper costs some.
        std::uint8_t basePrivacy;
        // Name of a building made mostly of rooms with this function.
        std::string_view buildingName;
    };

    const RoomFunctionInfo &roomFunctionInfo(RoomFunction function);
    std::span<const RoomFunctionInfo> roomFunctionCatalogue();
    std::string_view toString(RoomFunction function);
    // Room functions a piece of furniture serves or is required by.
    std::vector<RoomFunction> functionsUsing(FurnitureKind kind);

    // How well a room works for its designation. Separates capacity, privacy, furnishing, condition and comfort,
    // which will later feed into standard of living.
    struct RoomStatus
    {
        RoomFunction function = RoomFunction::Unassigned;
        // Enclosed, designated and with every required piece completed.
        bool functional = false;
        // Requirements not met (static descriptions from the catalogue).
        std::vector<std::string_view> missing;
        // First function (in catalogue order) whose requirements the furniture already meets; a suggestion only.
        RoomFunction suggested = RoomFunction::Unassigned;

        std::uint32_t tiles = 0;
        // Completed furniture inside, and pieces still being planned or built.
        std::uint32_t furniture = 0;
        std::uint32_t unfinishedFurniture = 0;
        // Capacities counted from the furniture the function uses (0 while not functional).
        std::uint32_t sleepers = 0;
        std::uint32_t storageKg = 0;
        std::uint32_t workplaces = 0;
        std::uint32_t seats = 0;
        // Sleepers beyond the function's limit (not counted).
        std::uint32_t crowding = 0;
        RoomService service = RoomService::None;
        // Amount of the service: sleepers, kg, places, seats, worshippers or patients.
        std::uint32_t serviceCapacity = 0;

        // 0..100 each.
        std::uint8_t privacy = 0;
        // Sum of the furniture's comfort points.
        std::uint32_t furnishing = 0;
        // Sturdiness of the walls: 100 for rough stone.
        std::uint8_t condition = 0;
        // Potential comfort for its sleepers or users: space, furnishing, privacy and roof.
        std::uint8_t comfort = 0;
        bool roofComplete = false;
    };

    // Null-safe: an unknown room gives a default (non-functional) status.
    RoomStatus evaluateRoom(const ConstructionState &construction, RoomId room);

    // Summary of a building from its rooms.
    struct BuildingSummary
    {
        // Name from its main function: "House" if it has a working bedroom, else the function with most tiles.
        std::string_view name;
        RoomFunction main = RoomFunction::Unassigned;
        std::uint32_t rooms = 0;
        std::uint32_t tiles = 0;
        std::uint32_t privateSleepers = 0;
        std::uint32_t communalSleepers = 0;
    };

    BuildingSummary summarizeBuilding(const ConstructionState &construction, BuildingId building);

} // namespace olam
