#include "render/renderer/Renderer.h"

#include "core/logging/Log.h"
#include "render/texture/Texture.h"

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

    int Renderer::maxTextureSize() const
    {
        const SDL_PropertiesID properties = SDL_GetRendererProperties(m_renderer);
        return static_cast<int>(SDL_GetNumberProperty(properties, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0));
    }

    Texture Renderer::createTexture(int width, int height)
    {
        SDL_Texture *texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
        if (!texture)
        {
            logging::error(LogCategory::Render, "SDL_CreateTexture {}x{} failed: {}", width, height, SDL_GetError());
            return Texture{};
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
        return Texture(texture, width, height);
    }

    void Renderer::drawTexture(const Texture &texture, const Rect &source, const Rect &destination)
    {
        if (!texture.valid())
            return;
        const SDL_FRect src{source.x, source.y, source.w, source.h};
        const SDL_FRect dst{destination.x, destination.y, destination.w, destination.h};
        SDL_RenderTexture(m_renderer, texture.handle(), &src, &dst);
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
