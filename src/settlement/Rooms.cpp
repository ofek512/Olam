#include "settlement/Rooms.h"

#include "core/debug/Assert.h"
#include "core/serialization/ByteReader.h"
#include "core/serialization/ByteWriter.h"
#include "settlement/ConstructionState.h"

#include <algorithm>
#include <array>
#include <limits>

namespace olam
{

    namespace
    {

        // Room tile keys hold 16-bit coordinates; floods that leave this range are open ground.
        constexpr std::int32_t kMaxCoord = 0xFFFF;

        bool inKeyRange(LocalCoord tile) { return tile.x >= 0 && tile.y >= 0 && tile.x <= kMaxCoord && tile.y <= kMaxCoord; }

        std::uint32_t tileKey(LocalCoord tile)
        {
            OLAM_ASSERT(inKeyRange(tile));
            return (static_cast<std::uint32_t>(tile.y) << 16) | static_cast<std::uint32_t>(tile.x);
        }

        LocalCoord keyTile(std::uint32_t key)
        {
            return {static_cast<std::int32_t>(key & 0xFFFFu), static_cast<std::int32_t>(key >> 16)};
        }

        // The neighbour across each edge of tileEdges(): north, east, south, west.
        constexpr std::array<LocalCoord, 4> kSteps = {{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

        LocalCoord neighbour(LocalCoord tile, std::size_t side) { return {tile.x + kSteps[side].x, tile.y + kSteps[side].y}; }

        // The completed wall or door on `edge`; null for an open edge (unfinished structures are openings).
        const Structure *boundary(const ConstructionState &construction, LocalEdge edge)
        {
            const Structure *s = construction.structure(construction.edgeAt(edge));
            return s && s->stage == ConstructionStage::Completed ? s : nullptr;
        }

        // Breadth-first flood from `start` across open edges, collecting tile keys in `reached`. False (open ground)
        // once more than `maxTiles` tiles are reached, the flood leaves the key range or it meets a tile already
        // known to be `outside`.
        bool flood(const ConstructionState &construction, LocalCoord start, int maxTiles, const std::set<std::uint32_t> &outside,
                   std::set<std::uint32_t> &reached)
        {
            std::vector<LocalCoord> queue{start};
            reached.insert(tileKey(start));
            for (std::size_t next = 0; next < queue.size(); ++next)
            {
                const LocalCoord tile = queue[next];
                const auto edges = tileEdges(tile);
                for (std::size_t side = 0; side < edges.size(); ++side)
                {
                    if (boundary(construction, edges[side]))
                        continue;
                    const LocalCoord other = neighbour(tile, side);
                    if (!inKeyRange(other) || outside.contains(tileKey(other)))
                        return false;
                    if (!reached.insert(tileKey(other)).second)
                        continue;
                    if (reached.size() > static_cast<std::size_t>(maxTiles))
                        return false;
                    queue.push_back(other);
                }
            }
            return true;
        }

        // Outline, bounding box and roof support of the room made of `tiles` (row-major).
        Room measure(const ConstructionState &construction, std::vector<LocalCoord> tiles)
        {
            Room room;
            room.tiles = std::move(tiles);
            room.min = room.tiles.front();
            room.max = room.tiles.front();
            std::vector<std::uint32_t> keys;
            keys.reserve(room.tiles.size());
            for (const LocalCoord tile : room.tiles)
            {
                keys.push_back(tileKey(tile));
                room.min = {std::min(room.min.x, tile.x), std::min(room.min.y, tile.y)};
                room.max = {std::max(room.max.x, tile.x), std::max(room.max.y, tile.y)};
            }
            const auto indexOf = [&](LocalCoord tile) -> std::optional<std::size_t>
            {
                if (!inKeyRange(tile))
                    return std::nullopt;
                const auto it = std::lower_bound(keys.begin(), keys.end(), tileKey(tile));
                if (it == keys.end() || *it != tileKey(tile))
                    return std::nullopt;
                return static_cast<std::size_t>(it - keys.begin());
            };

            // Steps from each tile to the nearest tile along a wall: a breadth-first search from all of those.
            constexpr int kUnreached = std::numeric_limits<int>::max();
            std::vector<int> distance(room.tiles.size(), kUnreached);
            std::vector<std::size_t> queue;
            for (std::size_t i = 0; i < room.tiles.size(); ++i)
            {
                const auto edges = tileEdges(room.tiles[i]);
                for (std::size_t side = 0; side < edges.size(); ++side)
                {
                    const Structure *s = boundary(construction, edges[side]);
                    if (!s)
                        continue;
                    if (s->kind == StructureKind::Wall && distance[i] != 0)
                    {
                        distance[i] = 0;
                        queue.push_back(i);
                    }
                    // A wall with the room on both sides (a partition stub) is not part of the outline.
                    if (indexOf(neighbour(room.tiles[i], side)))
                        continue;
                    if (s->kind == StructureKind::Door)
                        ++room.doors;
                    else
                        ++room.walls;
                }
            }
            for (std::size_t next = 0; next < queue.size(); ++next)
            {
                const std::size_t i = queue[next];
                const auto edges = tileEdges(room.tiles[i]);
                for (std::size_t side = 0; side < edges.size(); ++side)
                {
                    if (boundary(construction, edges[side]))
                        continue;
                    const auto j = indexOf(neighbour(room.tiles[i], side));
                    if (j && distance[*j] == kUnreached)
                    {
                        distance[*j] = distance[i] + 1;
                        queue.push_back(*j);
                    }
                }
            }
            const int reach = construction.config().maxRoofSupportDistance;
            room.roofSupported = std::all_of(distance.begin(), distance.end(), [&](int d) { return d <= reach; });
            return room;
        }

        // Tiles (rooms) or rooms (buildings) a new group shares with each old id, by old id.
        using Overlaps = std::map<std::uint32_t, std::size_t>;

        // Hands old ids to new groups by overlap, largest first (ties: lower old id, then earlier group); each old id
        // goes to at most one group. 0 where a group keeps no id.
        std::vector<std::uint32_t> inheritIds(const std::vector<Overlaps> &overlaps)
        {
            struct Candidate
            {
                std::size_t overlap;
                std::uint32_t id;
                std::size_t group;
            };
            std::vector<Candidate> candidates;
            for (std::size_t group = 0; group < overlaps.size(); ++group)
            {
                for (const auto &[id, overlap] : overlaps[group])
                    candidates.push_back({overlap, id, group});
            }
            std::sort(candidates.begin(), candidates.end(),
                      [](const Candidate &a, const Candidate &b)
                      {
                          if (a.overlap != b.overlap)
                              return a.overlap > b.overlap;
                          return a.id != b.id ? a.id < b.id : a.group < b.group;
                      });
            std::vector<std::uint32_t> ids(overlaps.size(), 0);
            std::set<std::uint32_t> used;
            for (const Candidate &c : candidates)
            {
                if (ids[c.group] == 0 && !used.contains(c.id))
                {
                    ids[c.group] = c.id;
                    used.insert(c.id);
                }
            }
            return ids;
        }

        // The old id a group overlaps most (the lower on ties); 0 if none.
        std::uint32_t largestOverlap(const Overlaps &overlaps)
        {
            std::uint32_t best = 0;
            std::size_t most = 0;
            for (const auto &[id, overlap] : overlaps)
            {
                if (overlap > most)
                {
                    best = id;
                    most = overlap;
                }
            }
            return best;
        }

    } // namespace

    std::string_view toString(RoofMode mode)
    {
        switch (mode)
        {
        case RoofMode::Auto:
            return "auto";
        case RoofMode::Enabled:
            return "always";
        case RoofMode::Disabled:
            return "never";
        case RoofMode::Count:
            break;
        }
        return "?";
    }

    const Room *RoomIndex::room(RoomId id) const
    {
        const auto it = m_rooms.find(id.value);
        return it != m_rooms.end() ? &it->second : nullptr;
    }

    RoomId RoomIndex::roomAt(LocalCoord tile) const
    {
        if (!inKeyRange(tile))
            return {};
        const auto it = m_tiles.find(tileKey(tile));
        return it != m_tiles.end() ? it->second : RoomId{};
    }

    const Building *RoomIndex::building(BuildingId id) const
    {
        const auto it = m_buildings.find(id.value);
        return it != m_buildings.end() ? &it->second : nullptr;
    }

    std::size_t RoomIndex::roofedCount() const
    {
        return static_cast<std::size_t>(
            std::count_if(m_rooms.begin(), m_rooms.end(), [](const auto &entry) { return entry.second.roofed(); }));
    }

    std::vector<RoomId> RoomIndex::roomIds() const
    {
        std::vector<RoomId> ids;
        ids.reserve(m_rooms.size());
        for (const auto &[id, room] : m_rooms)
            ids.push_back(RoomId{id});
        return ids;
    }

    std::vector<RoomRun> RoomIndex::runsInRect(LocalCoord a, LocalCoord b) const
    {
        const int x0 = std::clamp(std::min(a.x, b.x), 0, kMaxCoord);
        const int y0 = std::clamp(std::min(a.y, b.y), 0, kMaxCoord);
        const int x1 = std::clamp(std::max(a.x, b.x), 0, kMaxCoord);
        const int y1 = std::clamp(std::max(a.y, b.y), 0, kMaxCoord);
        std::vector<RoomRun> runs;
        // Keys are row-major, so every row is one index range.
        for (int y = y0; y <= y1; ++y)
        {
            const auto last = m_tiles.upper_bound(tileKey({x1, y}));
            for (auto it = m_tiles.lower_bound(tileKey({x0, y})); it != last; ++it)
            {
                const LocalCoord tile = keyTile(it->first);
                if (!runs.empty() && runs.back().y == y && runs.back().room == it->second && runs.back().x1 + 1 == tile.x)
                    runs.back().x1 = tile.x;
                else
                    runs.push_back({y, tile.x, tile.x, it->second});
            }
        }
        return runs;
    }

    void RoomIndex::markDirty(LocalEdge edge)
    {
        for (const LocalCoord tile : edgeTiles(edge))
            m_dirty.insert(tileKey(tile));
    }

    void RoomIndex::update(const ConstructionState &construction)
    {
        const std::set<std::uint32_t> dirty = std::move(m_dirty);
        m_dirty.clear();

        // Rooms next to a changed edge are taken apart; their tiles remember the old room for identity. Every tile
        // of such a room is reached again from some dirty tile, since a path inside the old room can only be cut
        // at a changed edge, whose two tiles are both dirty.
        std::map<std::uint32_t, Room> old;
        std::map<std::uint32_t, std::uint32_t> oldRoomOf;
        for (const std::uint32_t key : dirty)
        {
            const auto it = m_tiles.find(key);
            if (it == m_tiles.end())
                continue;
            const std::uint32_t id = it->second.value;
            auto node = m_rooms.extract(id);
            for (const LocalCoord tile : node.mapped().tiles)
            {
                m_tiles.erase(tileKey(tile));
                oldRoomOf.emplace(tileKey(tile), id);
            }
            old.emplace(id, std::move(node.mapped()));
        }

        // Flood again from each dirty tile not reached yet. Tiles of floods that escape are open ground, which
        // stops later floods early.
        std::vector<std::vector<LocalCoord>> regions;
        std::set<std::uint32_t> outside;
        std::set<std::uint32_t> enclosed;
        const int maxTiles = construction.config().maxRoomTiles;
        for (const std::uint32_t key : dirty)
        {
            if (outside.contains(key) || enclosed.contains(key))
                continue;
            std::set<std::uint32_t> reached;
            if (!flood(construction, keyTile(key), maxTiles, outside, reached))
            {
                outside.merge(reached);
                continue;
            }
            std::vector<LocalCoord> &tiles = regions.emplace_back();
            tiles.reserve(reached.size());
            for (const std::uint32_t tile : reached)
                tiles.push_back(keyTile(tile));
            enclosed.merge(reached);
        }

        // Ids and roof settings pass to the regions from the old rooms they overlap most.
        std::vector<Overlaps> overlaps(regions.size());
        for (std::size_t r = 0; r < regions.size(); ++r)
        {
            for (const LocalCoord tile : regions[r])
            {
                if (const auto it = oldRoomOf.find(tileKey(tile)); it != oldRoomOf.end())
                    ++overlaps[r][it->second];
            }
        }
        const std::vector<std::uint32_t> kept = inheritIds(overlaps);
        std::set<std::uint32_t> pending;
        for (std::size_t r = 0; r < regions.size(); ++r)
        {
            Room room = measure(construction, std::move(regions[r]));
            // A part split off a room starts with its settings, and counts towards its building below.
            if (const std::uint32_t source = kept[r] != 0 ? kept[r] : largestOverlap(overlaps[r]); source != 0)
            {
                room.roof = old.at(source).roof;
                room.building = old.at(source).building;
            }
            const RoomId id{kept[r] != 0 ? kept[r] : m_nextRoomId++};
            for (const LocalCoord tile : room.tiles)
                m_tiles.emplace(tileKey(tile), id);
            m_rooms.emplace(id.value, std::move(room));
            pending.insert(id.value);
        }

        // Buildings are regrouped for every new room and every room of a building that lost or changed a room.
        std::set<std::uint32_t> touched;
        for (const auto &[id, room] : old)
        {
            if (room.building.isValid())
                touched.insert(room.building.value);
        }
        for (const std::uint32_t b : touched)
        {
            if (const Building *building = this->building(BuildingId{b}))
            {
                for (const RoomId r : building->rooms)
                {
                    if (m_rooms.contains(r.value))
                        pending.insert(r.value);
                }
            }
        }
        // Rooms on the other side of a room's doors.
        const auto doorNeighbours = [&](std::uint32_t id)
        {
            std::vector<std::uint32_t> neighbours;
            for (const LocalCoord tile : m_rooms.at(id).tiles)
            {
                const auto edges = tileEdges(tile);
                for (std::size_t side = 0; side < edges.size(); ++side)
                {
                    const Structure *s = boundary(construction, edges[side]);
                    if (!s || s->kind != StructureKind::Door)
                        continue;
                    const RoomId other = roomAt(neighbour(tile, side));
                    if (other.isValid() && other.value != id)
                        neighbours.push_back(other.value);
                }
            }
            return neighbours;
        };
        std::vector<std::vector<std::uint32_t>> groups;
        std::set<std::uint32_t> grouped;
        for (const std::uint32_t start : pending)
        {
            if (!grouped.insert(start).second)
                continue;
            std::vector<std::uint32_t> &group = groups.emplace_back(1, start);
            for (std::size_t next = 0; next < group.size(); ++next)
            {
                // The group may reach rooms of untouched buildings (a new door joins them); those are regrouped too.
                for (const std::uint32_t other : doorNeighbours(group[next]))
                {
                    if (grouped.insert(other).second)
                        group.push_back(other);
                }
            }
            std::sort(group.begin(), group.end());
        }
        std::vector<Overlaps> shares(groups.size());
        for (std::size_t g = 0; g < groups.size(); ++g)
        {
            for (const std::uint32_t r : groups[g])
            {
                const Room &room = m_rooms.at(r);
                if (!room.building.isValid())
                    continue;
                shares[g][room.building.value] += room.tiles.size();
                touched.insert(room.building.value);
            }
        }
        const std::vector<std::uint32_t> keptBuildings = inheritIds(shares);
        for (const std::uint32_t b : touched)
            m_buildings.erase(b);
        for (std::size_t g = 0; g < groups.size(); ++g)
        {
            const BuildingId id{keptBuildings[g] != 0 ? keptBuildings[g] : m_nextBuildingId++};
            Building &building = m_buildings[id.value];
            for (const std::uint32_t r : groups[g])
            {
                m_rooms.at(r).building = id;
                building.rooms.push_back(RoomId{r});
            }
        }
    }

    bool RoomIndex::setRoof(RoomId id, RoofSettings roof)
    {
        const auto it = m_rooms.find(id.value);
        if (it == m_rooms.end() || roof.mode >= RoofMode::Count || !isRoofMaterial(roof.material))
            return false;
        it->second.roof = roof;
        return true;
    }

    void RoomIndex::write(ByteWriter &writer) const
    {
        writer.write(m_nextRoomId);
        writer.write(m_nextBuildingId);
        writer.write(static_cast<std::uint32_t>(m_rooms.size()));
        for (const auto &[id, room] : m_rooms)
        {
            // The tiles follow from the structures; the first one identifies the room when loading.
            writer.write(id);
            writer.write(room.building);
            writer.write(room.tiles.front().x);
            writer.write(room.tiles.front().y);
            writer.write(room.roof.mode);
            writer.write(room.roof.material);
        }
    }

    std::optional<std::string> RoomIndex::restore(ByteReader &reader)
    {
        std::uint32_t nextRoomId = 0;
        std::uint32_t nextBuildingId = 0;
        std::uint32_t count = 0;
        reader.read(nextRoomId);
        reader.read(nextBuildingId);
        reader.read(count);
        if (reader.failed())
            return "truncated room list";
        if (count != m_rooms.size())
            return "rooms do not match the structures";

        std::map<std::uint32_t, Room> rooms;
        std::map<std::uint32_t, RoomId> tiles;
        std::map<std::uint32_t, Building> buildings;
        std::set<std::uint32_t> claimed;
        // Detected building -> saved building, one to one.
        std::map<std::uint32_t, std::uint32_t> savedBuilding;
        std::set<std::uint32_t> usedBuildings;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            std::uint32_t id = 0;
            BuildingId building;
            LocalCoord anchor;
            RoofSettings roof;
            reader.read(id);
            reader.read(building);
            reader.read(anchor.x);
            reader.read(anchor.y);
            reader.read(roof.mode);
            reader.read(roof.material);
            if (reader.failed())
                return "truncated room list";
            if (id == 0 || id >= nextRoomId || rooms.contains(id) || !building.isValid() || building.value >= nextBuildingId ||
                roof.mode >= RoofMode::Count || !isRoofMaterial(roof.material))
                return "corrupt room entry";
            const RoomId detectedId = roomAt(anchor);
            const Room *detected = room(detectedId);
            if (!detected || !(detected->tiles.front() == anchor) || !claimed.insert(detectedId.value).second)
                return "rooms do not match the structures";
            const auto [entry, added] = savedBuilding.emplace(detected->building.value, building.value);
            if (added ? !usedBuildings.insert(building.value).second : entry->second != building.value)
                return "buildings do not match the structures";

            Room &restored = rooms.emplace(id, *detected).first->second;
            restored.building = building;
            restored.roof = roof;
            for (const LocalCoord tile : restored.tiles)
                tiles.emplace(tileKey(tile), RoomId{id});
            buildings[building.value].rooms.push_back(RoomId{id});
        }
        for (auto &[id, building] : buildings)
            std::sort(building.rooms.begin(), building.rooms.end());
        m_rooms = std::move(rooms);
        m_tiles = std::move(tiles);
        m_buildings = std::move(buildings);
        m_nextRoomId = nextRoomId;
        m_nextBuildingId = nextBuildingId;
        return std::nullopt;
    }

} // namespace olam
