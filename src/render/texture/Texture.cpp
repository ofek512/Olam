#include "render/texture/Texture.h"

#include "core/debug/Assert.h"

#include <SDL3/SDL.h>

#include <utility>

namespace olam
{

    Texture::Texture(SDL_Texture *texture, int width, int height)
        : m_texture(texture), m_width(width), m_height(height)
    {
    }

    Texture::~Texture()
    {
        destroy();
    }

    Texture::Texture(Texture &&other) noexcept
        : m_texture(std::exchange(other.m_texture, nullptr)), m_width(other.m_width), m_height(other.m_height)
    {
    }

    Texture &Texture::operator=(Texture &&other) noexcept
    {
        if (this != &other)
        {
            destroy();
            m_texture = std::exchange(other.m_texture, nullptr);
            m_width = other.m_width;
            m_height = other.m_height;
        }
        return *this;
    }

    void Texture::update(std::span<const std::uint8_t> rgba)
    {
        if (!m_texture)
            return;
        OLAM_ASSERT(rgba.size() == static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height) * 4);
        SDL_UpdateTexture(m_texture, nullptr, rgba.data(), m_width * 4);
    }

    void Texture::destroy()
    {
        if (m_texture)
        {
            SDL_DestroyTexture(m_texture);
            m_texture = nullptr;
        }
    }

} // namespace olam
