#include "render/ui/UiRenderer.h"

#include "render/renderer/Renderer.h"

namespace olam
{

    void drawUi(Renderer &renderer, std::span<const UiDraw> draws)
    {
        for (const UiDraw &draw : draws)
        {
            const Color color{draw.color[0], draw.color[1], draw.color[2], draw.color[3]};
            switch (draw.kind)
            {
            case UiDraw::Kind::Fill:
                renderer.fillRect(draw.rect, color);
                break;
            case UiDraw::Kind::Outline:
                renderer.drawRect(draw.rect, color);
                break;
            case UiDraw::Kind::Text:
                renderer.drawDebugText({draw.rect.x, draw.rect.y}, draw.text, color, draw.scale);
                break;
            }
        }
    }

} // namespace olam
