#pragma once

#include <string>

struct SDL_Window;
struct SDL_Renderer;

namespace olam
{

    class Input;

    struct WindowConfig
    {
        std::string title = "Olam";
        int width = 1600;
        int height = 900;
        bool vsync = true;
    };

    // Owns SDL initialisation, the window and the SDL renderer, and translates SDL events into Input.
    class SdlPlatform
    {
    public:
        SdlPlatform() = default;
        ~SdlPlatform();
        SdlPlatform(const SdlPlatform &) = delete;
        SdlPlatform &operator=(const SdlPlatform &) = delete;

        bool initialize(const WindowConfig &config);
        void shutdown();

        void pumpEvents(Input &input);
        bool quitRequested() const { return m_quitRequested; }

        void setWindowTitle(const std::string &title);

        SDL_Window *window() const { return m_window; }
        SDL_Renderer *renderer() const { return m_renderer; }

    private:
        SDL_Window *m_window = nullptr;
        SDL_Renderer *m_renderer = nullptr;
        bool m_sdlInitialized = false;
        bool m_quitRequested = false;
    };

} // namespace olam
