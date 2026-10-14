#pragma once

#include "core/types/StrongId.h"
#include "settlement/Goods.h"
#include "settlement/LocalTypes.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace olam
{

    class ByteReader;
    class ByteWriter;
    class ConstructionState;

    struct StockpileTag;
    using StockpileId = StrongId<StockpileTag>;
    struct ConstructionOrderTag;
    using OrderId = StrongId<ConstructionOrderTag>;

    // A marked storage area on the local map holding goods (Phase 4B.5).
    struct Stockpile
    {
        // Tile rectangle (inclusive corners).
        LocalCoord min;
        LocalCoord max;
        GoodAmounts stored;

        LocalCoord center() const { return {(min.x + max.x) / 2, (min.y + max.y) / 2}; }
        bool contains(LocalCoord tile) const { return tile.x >= min.x && tile.x <= max.x && tile.y >= min.y && tile.y <= max.y; }
    };

    // Physical storage: goods belong to stockpiles, and construction orders reserve them there until they are
    // delivered to the site, so reserved goods are never available to anyone else. Owned by ConstructionState,
    // which keeps reservations, deliveries and refunds consistent with its orders.
    class Stockpiles
    {
    public:
        // Null for an unknown or removed id.
        const Stockpile *stockpile(StockpileId id) const;
        StockpileId stockpileAt(LocalCoord tile) const;
        std::size_t count() const { return m_stockpiles.size(); }
        // Ascending.
        std::vector<StockpileId> ids() const;

        // Held for orders, and what is left for new reservations.
        GoodAmounts reserved(StockpileId id) const;
        GoodAmounts available(StockpileId id) const;
        // Everything an order holds in reservations.
        GoodAmounts reservedFor(OrderId order) const;
        GoodAmounts totalStored() const;
        GoodAmounts totalAvailable() const;
        // Number of (stockpile, order) reservations.
        std::size_t reservationCount() const { return m_reservations.size(); }

    private:
        friend class ConstructionState;

        StockpileId add(LocalCoord min, LocalCoord max);
        // Drops the stockpile with its goods and reservations.
        void erase(StockpileId id);
        void store(StockpileId id, const GoodAmounts &goods);
        // Reserves up to `need` for `order`, nearest stockpiles first; returns what was reserved.
        GoodAmounts reserve(OrderId order, GoodAmounts need, LocalCoord near);
        // Releases up to `amount` of the order's reservations, farthest stockpiles first.
        void release(OrderId order, GoodAmounts amount, LocalCoord near);
        void releaseAll(OrderId order);
        // Takes the order's reserved goods out of their stockpiles; `source` becomes the nearest one they came from.
        GoodAmounts take(OrderId order, LocalCoord near, StockpileId &source);
        // Takes unreserved goods directly (all or nothing), nearest stockpiles first.
        bool consume(const GoodAmounts &goods, LocalCoord near);
        // Stockpiles by distance from `near` (then id).
        std::vector<std::uint32_t> byDistance(LocalCoord near) const;
        void write(ByteWriter &writer) const;
        std::optional<std::string> read(ByteReader &reader);

        std::map<std::uint32_t, Stockpile> m_stockpiles;
        // (stockpile, order) -> reserved goods.
        std::map<std::pair<std::uint32_t, std::uint32_t>, GoodAmounts> m_reservations;
        std::uint32_t m_nextId = 1;
    };

} // namespace olam
