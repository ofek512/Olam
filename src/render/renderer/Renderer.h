#pragma once

#include "core/math/Vec2.h"

#include <cstdint>
#include <string>

struct SDL_Renderer;

namespace olam
{

    class Texture;

    struct Color
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        std::uint8_t a = 255;
    };

    // Thin wrapper over the SDL renderer. All coordinates are screen pixels.
    class Renderer
    {
    public:
        static constexpr float kDebugGlyphSize = 8.0f;

        explicit Renderer(SDL_Renderer *renderer);

        void beginFrame(Color clearColor);
        void endFrame();

        Vec2 outputSize() const;
        int maxTextureSize() const;

        // RGBA, nearest filtering. Returns an invalid texture on failure.
        Texture createTexture(int width, int height);
        void drawTexture(const Texture &texture, const Rect &source, const Rect &destination);

        void fillRect(const Rect &rect, Color color);
        void drawRect(const Rect &rect, Color color);
        void drawLine(Vec2 from, Vec2 to, Color color);

        // Built-in SDL 8x8 bitmap font; for debug output only.
        void drawDebugText(Vec2 position, const std::string &text, Color color, float scale = 1.0f);

    private:
        void setColor(Color color);

        SDL_Renderer *m_renderer;
    };

} // namespace olam
