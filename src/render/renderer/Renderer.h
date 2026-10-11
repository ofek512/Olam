#pragma once

#include "core/math/Vec2.h"

#include <cstdint>
#include <string>

struct SDL_Renderer;

namespace olam
{

    class Texture;
    class SpriteBatch;

    struct Color
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        std::uint8_t a = 255;
    };

    struct TextureOptions
    {
        bool linear = false;
        // Alpha blending (sprites); opaque textures skip it.
        bool blend = false;
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

        // RGBA, nearest filtering and no blending unless requested. Returns an invalid texture on failure.
        Texture createTexture(int width, int height, TextureOptions options = {});
        void drawTexture(const Texture &texture, const Rect &source, const Rect &destination);
        // All quads of the batch in one draw call.
        void drawBatch(const Texture &texture, const SpriteBatch &batch);

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
