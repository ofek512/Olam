#include "world/TileView.h"

#include "core/debug/Assert.h"
#include "world/World.h"

namespace olam
{

    TileView::TileView(const World &world, WorldCoord coord)
        : m_world(&world), m_coord(coord)
    {
        OLAM_ASSERT(world.isValid(coord));
    }

    std::size_t TileView::index() const
    {
        return m_world->index(m_coord);
    }

    double TileView::latitude() const
    {
        return m_world->latitudeAt(m_coord.y);
    }

} // namespace olam
