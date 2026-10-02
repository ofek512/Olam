#pragma once

#include <cstdint>
#include <span>

struct SDL_Texture;

namespace olam
{

    // Owning, move-only wrapper over an SDL texture. Created via Renderer::createTexture.
    class Texture
    {
    public:
        Texture() = default;
        Texture(SDL_Texture *texture, int width, int height);
        ~Texture();

        Texture(Texture &&other) noexcept;
        Texture &operator=(Texture &&other) noexcept;
        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;

        bool valid() const { return m_texture != nullptr; }
        int width() const { return m_width; }
        int height() const { return m_height; }
        SDL_Texture *handle() const { return m_texture; }

        // Replaces all pixels; expects width * height RGBA8 pixels, rows tightly packed.
        void update(std::span<const std::uint8_t> rgba);

    private:
        void destroy();

        SDL_Texture *m_texture = nullptr;
        int m_width = 0;
        int m_height = 0;
    };

} // namespace olam
