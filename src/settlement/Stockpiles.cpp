#include "settlement/Stockpiles.h"

#include "core/debug/Assert.h"
#include "core/serialization/ByteReader.h"
#include "core/serialization/ByteWriter.h"

#include <algorithm>
#include <cstdint>

namespace olam
{

    namespace
    {

        std::int64_t distanceSquared(LocalCoord a, LocalCoord b)
        {
            const std::int64_t dx = a.x - b.x;
            const std::int64_t dy = a.y - b.y;
            return dx * dx + dy * dy;
        }

        void writeGoods(ByteWriter &writer, const GoodAmounts &goods)
        {
            for (const std::uint32_t kg : goods.kg)
                writer.write(kg);
        }

        void readGoods(ByteReader &reader, GoodAmounts &goods)
        {
            for (std::uint32_t &kg : goods.kg)
                reader.read(kg);
        }

    } // namespace

    const Stockpile *Stockpiles::stockpile(StockpileId id) const
    {
        const auto it = m_stockpiles.find(id.value);
        return it != m_stockpiles.end() ? &it->second : nullptr;
    }

    StockpileId Stockpiles::stockpileAt(LocalCoord tile) const
    {
        for (const auto &[id, stockpile] : m_stockpiles)
        {
            if (stockpile.contains(tile))
                return StockpileId{id};
        }
        return {};
    }

    std::vector<StockpileId> Stockpiles::ids() const
    {
        std::vector<StockpileId> ids;
        for (const auto &[id, stockpile] : m_stockpiles)
            ids.push_back(StockpileId{id});
        return ids;
    }

    GoodAmounts Stockpiles::reserved(StockpileId id) const
    {
        GoodAmounts total;
        for (auto it = m_reservations.lower_bound({id.value, 0}); it != m_reservations.end() && it->first.first == id.value; ++it)
            total += it->second;
        return total;
    }

    GoodAmounts Stockpiles::available(StockpileId id) const
    {
        const Stockpile *s = stockpile(id);
        return s ? s->stored - reserved(id) : GoodAmounts{};
    }

    GoodAmounts Stockpiles::reservedFor(OrderId order) const
    {
        GoodAmounts total;
        for (const auto &[key, goods] : m_reservations)
        {
            if (key.second == order.value)
                total += goods;
        }
        return total;
    }

    GoodAmounts Stockpiles::totalStored() const
    {
        GoodAmounts total;
        for (const auto &[id, stockpile] : m_stockpiles)
            total += stockpile.stored;
        return total;
    }

    GoodAmounts Stockpiles::totalAvailable() const
    {
        GoodAmounts total;
        for (const auto &[id, stockpile] : m_stockpiles)
            total += available(StockpileId{id});
        return total;
    }

    StockpileId Stockpiles::add(LocalCoord min, LocalCoord max)
    {
        const StockpileId id{m_nextId++};
        m_stockpiles.emplace(id.value, Stockpile{min, max, {}});
        return id;
    }

    void Stockpiles::erase(StockpileId id)
    {
        std::erase_if(m_reservations, [&](const auto &entry) { return entry.first.first == id.value; });
        m_stockpiles.erase(id.value);
    }

    void Stockpiles::store(StockpileId id, const GoodAmounts &goods)
    {
        OLAM_ASSERT(m_stockpiles.contains(id.value));
        m_stockpiles.at(id.value).stored += goods;
    }

    std::vector<std::uint32_t> Stockpiles::byDistance(LocalCoord near) const
    {
        std::vector<std::pair<std::int64_t, std::uint32_t>> order;
        for (const auto &[id, stockpile] : m_stockpiles)
            order.emplace_back(distanceSquared(stockpile.center(), near), id);
        std::sort(order.begin(), order.end());
        std::vector<std::uint32_t> ids;
        for (const auto &[distance, id] : order)
            ids.push_back(id);
        return ids;
    }

    GoodAmounts Stockpiles::reserve(OrderId order, GoodAmounts need, LocalCoord near)
    {
        GoodAmounts reserved;
        for (const std::uint32_t id : byDistance(near))
        {
            if (need.empty())
                break;
            const GoodAmounts amount = minimum(available(StockpileId{id}), need);
            if (amount.empty())
                continue;
            m_reservations[{id, order.value}] += amount;
            need -= amount;
            reserved += amount;
        }
        return reserved;
    }

    void Stockpiles::release(OrderId order, GoodAmounts amount, LocalCoord near)
    {
        std::vector<std::uint32_t> ids = byDistance(near);
        std::reverse(ids.begin(), ids.end());
        for (const std::uint32_t id : ids)
        {
            if (amount.empty())
                break;
            const auto it = m_reservations.find({id, order.value});
            if (it == m_reservations.end())
                continue;
            const GoodAmounts released = minimum(it->second, amount);
            it->second -= released;
            amount -= released;
            if (it->second.empty())
                m_reservations.erase(it);
        }
    }

    void Stockpiles::releaseAll(OrderId order)
    {
        std::erase_if(m_reservations, [&](const auto &entry) { return entry.first.second == order.value; });
    }

    GoodAmounts Stockpiles::take(OrderId order, LocalCoord near, StockpileId &source)
    {
        GoodAmounts taken;
        for (const std::uint32_t id : byDistance(near))
        {
            const auto it = m_reservations.find({id, order.value});
            if (it == m_reservations.end())
                continue;
            Stockpile &stockpile = m_stockpiles.at(id);
            // Reservations never exceed what is stored.
            OLAM_ASSERT(stockpile.stored.covers(it->second));
            stockpile.stored -= it->second;
            taken += it->second;
            if (!source.isValid())
                source = StockpileId{id};
            m_reservations.erase(it);
        }
        return taken;
    }

    bool Stockpiles::consume(const GoodAmounts &goods, LocalCoord near)
    {
        if (!totalAvailable().covers(goods))
            return false;
        GoodAmounts remaining = goods;
        for (const std::uint32_t id : byDistance(near))
        {
            const GoodAmounts amount = minimum(available(StockpileId{id}), remaining);
            m_stockpiles.at(id).stored -= amount;
            remaining -= amount;
        }
        OLAM_ASSERT(remaining.empty());
        return true;
    }

    void Stockpiles::write(ByteWriter &writer) const
    {
        writer.write(m_nextId);
        writer.write(static_cast<std::uint32_t>(m_stockpiles.size()));
        for (const auto &[id, stockpile] : m_stockpiles)
        {
            writer.write(id);
            writer.write(stockpile.min.x);
            writer.write(stockpile.min.y);
            writer.write(stockpile.max.x);
            writer.write(stockpile.max.y);
            writeGoods(writer, stockpile.stored);
        }
        writer.write(static_cast<std::uint32_t>(m_reservations.size()));
        for (const auto &[key, goods] : m_reservations)
        {
            writer.write(key.first);
            writer.write(key.second);
            writeGoods(writer, goods);
        }
    }

    std::optional<std::string> Stockpiles::read(ByteReader &reader)
    {
        Stockpiles loaded;
        std::uint32_t count = 0;
        reader.read(loaded.m_nextId);
        reader.read(count);
        for (std::uint32_t i = 0; i < count && !reader.failed(); ++i)
        {
            std::uint32_t id = 0;
            Stockpile stockpile;
            reader.read(id);
            reader.read(stockpile.min.x);
            reader.read(stockpile.min.y);
            reader.read(stockpile.max.x);
            reader.read(stockpile.max.y);
            readGoods(reader, stockpile.stored);
            if (reader.failed())
                break;
            if (id == 0 || id >= loaded.m_nextId || loaded.m_stockpiles.contains(id) || stockpile.min.x > stockpile.max.x ||
                stockpile.min.y > stockpile.max.y)
                return "corrupt stockpile entry";
            loaded.m_stockpiles.emplace(id, stockpile);
        }
        reader.read(count);
        for (std::uint32_t i = 0; i < count && !reader.failed(); ++i)
        {
            std::uint32_t stockpile = 0;
            std::uint32_t order = 0;
            GoodAmounts goods;
            reader.read(stockpile);
            reader.read(order);
            readGoods(reader, goods);
            if (reader.failed())
                break;
            if (!loaded.m_stockpiles.contains(stockpile) || order == 0 || goods.empty() ||
                !loaded.m_reservations.emplace(std::pair{stockpile, order}, goods).second)
                return "corrupt stockpile reservation";
        }
        if (reader.failed())
            return "truncated stockpiles";
        for (const auto &[id, stockpile] : loaded.m_stockpiles)
        {
            if (!stockpile.stored.covers(loaded.reserved(StockpileId{id})))
                return "stockpile reserves more than it stores";
        }
        *this = std::move(loaded);
        return std::nullopt;
    }

} // namespace olam
