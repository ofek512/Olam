#include "render/renderer/Renderer.h"

#include <SDL3/SDL.h>

namespace olam
{

    Renderer::Renderer(SDL_Renderer *renderer)
        : m_renderer(renderer)
    {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    }

    void Renderer::beginFrame(Color clearColor)
    {
        setColor(clearColor);
        SDL_RenderClear(m_renderer);
    }

    void Renderer::endFrame()
    {
        SDL_RenderPresent(m_renderer);
    }

    Vec2 Renderer::outputSize() const
    {
        int w = 0;
        int h = 0;
        SDL_GetCurrentRenderOutputSize(m_renderer, &w, &h);
        return {static_cast<float>(w), static_cast<float>(h)};
    }

    void Renderer::fillRect(const Rect &rect, Color color)
    {
        setColor(color);
        const SDL_FRect r{rect.x, rect.y, rect.w, rect.h};
        SDL_RenderFillRect(m_renderer, &r);
    }

    void Renderer::drawRect(const Rect &rect, Color color)
    {
        setColor(color);
        const SDL_FRect r{rect.x, rect.y, rect.w, rect.h};
        SDL_RenderRect(m_renderer, &r);
    }

    void Renderer::drawLine(Vec2 from, Vec2 to, Color color)
    {
        setColor(color);
        SDL_RenderLine(m_renderer, from.x, from.y, to.x, to.y);
    }

    void Renderer::drawDebugText(Vec2 position, const std::string &text, Color color, float scale)
    {
        float oldScaleX = 1.0f;
        float oldScaleY = 1.0f;
        SDL_GetRenderScale(m_renderer, &oldScaleX, &oldScaleY);
        SDL_SetRenderScale(m_renderer, scale, scale);

        setColor(color);
        SDL_RenderDebugText(m_renderer, position.x / scale, position.y / scale, text.c_str());

        SDL_SetRenderScale(m_renderer, oldScaleX, oldScaleY);
    }

    void Renderer::setColor(Color color)
    {
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    }

} // namespace olam
