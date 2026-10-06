#include "platform/sdl/SdlPlatform.h"

#include "core/input/Input.h"
#include "core/logging/Log.h"

#include <SDL3/SDL.h>

#include <optional>

namespace olam
{

    namespace
    {

        Key mapScancode(SDL_Scancode scancode)
        {
            switch (scancode)
            {
            case SDL_SCANCODE_W:
                return Key::W;
            case SDL_SCANCODE_A:
                return Key::A;
            case SDL_SCANCODE_S:
                return Key::S;
            case SDL_SCANCODE_D:
                return Key::D;
            case SDL_SCANCODE_R:
                return Key::R;
            case SDL_SCANCODE_N:
                return Key::N;
            case SDL_SCANCODE_H:
                return Key::H;
            case SDL_SCANCODE_I:
                return Key::I;
            case SDL_SCANCODE_L:
                return Key::L;
            case SDL_SCANCODE_M:
                return Key::M;
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER:
                return Key::Enter;
            case SDL_SCANCODE_TAB:
                return Key::Tab;
            case SDL_SCANCODE_LCTRL:
                return Key::LeftCtrl;
            case SDL_SCANCODE_RCTRL:
                return Key::RightCtrl;
            case SDL_SCANCODE_UP:
                return Key::Up;
            case SDL_SCANCODE_DOWN:
                return Key::Down;
            case SDL_SCANCODE_LEFT:
                return Key::Left;
            case SDL_SCANCODE_RIGHT:
                return Key::Right;
            case SDL_SCANCODE_ESCAPE:
                return Key::Escape;
            case SDL_SCANCODE_SPACE:
                return Key::Space;
            case SDL_SCANCODE_PERIOD:
                return Key::Period;
            case SDL_SCANCODE_GRAVE:
                return Key::Grave;
            case SDL_SCANCODE_LSHIFT:
                return Key::LeftShift;
            case SDL_SCANCODE_RSHIFT:
                return Key::RightShift;
            case SDL_SCANCODE_F1:
                return Key::F1;
            case SDL_SCANCODE_F2:
                return Key::F2;
            case SDL_SCANCODE_F3:
                return Key::F3;
            case SDL_SCANCODE_F4:
                return Key::F4;
            case SDL_SCANCODE_F5:
                return Key::F5;
            case SDL_SCANCODE_F6:
                return Key::F6;
            case SDL_SCANCODE_F7:
                return Key::F7;
            case SDL_SCANCODE_F8:
                return Key::F8;
            case SDL_SCANCODE_F9:
                return Key::F9;
            case SDL_SCANCODE_F10:
                return Key::F10;
            case SDL_SCANCODE_F11:
                return Key::F11;
            case SDL_SCANCODE_F12:
                return Key::F12;
            default:
                return Key::Unknown;
            }
        }

        std::optional<MouseButton> mapMouseButton(Uint8 button)
        {
            switch (button)
            {
            case SDL_BUTTON_LEFT:
                return MouseButton::Left;
            case SDL_BUTTON_MIDDLE:
                return MouseButton::Middle;
            case SDL_BUTTON_RIGHT:
                return MouseButton::Right;
            default:
                return std::nullopt;
            }
        }

    } // namespace

    SdlPlatform::~SdlPlatform()
    {
        shutdown();
    }

    bool SdlPlatform::initialize(const WindowConfig &config)
    {
        const int version = SDL_GetVersion();
        logging::info(LogCategory::Platform, "SDL {}.{}.{}", SDL_VERSIONNUM_MAJOR(version), SDL_VERSIONNUM_MINOR(version),
                      SDL_VERSIONNUM_MICRO(version));

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            logging::error(LogCategory::Platform, "SDL_Init failed: {}", SDL_GetError());
            return false;
        }
        m_sdlInitialized = true;

        if (!SDL_CreateWindowAndRenderer(config.title.c_str(), config.width, config.height, SDL_WINDOW_RESIZABLE, &m_window,
                                         &m_renderer))
        {
            logging::error(LogCategory::Platform, "SDL_CreateWindowAndRenderer failed: {}", SDL_GetError());
            return false;
        }

        if (!SDL_SetRenderVSync(m_renderer, config.vsync ? 1 : SDL_RENDERER_VSYNC_DISABLED))
            logging::warn(LogCategory::Platform, "Could not set vsync: {}", SDL_GetError());

        logging::info(LogCategory::Platform, "Window {}x{}, renderer '{}', vsync {}", config.width, config.height,
                      SDL_GetRendererName(m_renderer), config.vsync ? "on" : "off");
        return true;
    }

    void SdlPlatform::shutdown()
    {
        if (m_renderer)
        {
            SDL_DestroyRenderer(m_renderer);
            m_renderer = nullptr;
        }
        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }
        if (m_sdlInitialized)
        {
            SDL_Quit();
            m_sdlInitialized = false;
            logging::info(LogCategory::Platform, "SDL shut down");
        }
    }

    void SdlPlatform::pumpEvents(Input &input)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // Mouse coordinates in render-output pixels, matching what the renderer and camera use.
            if (m_renderer)
                SDL_ConvertEventToRenderCoordinates(m_renderer, &event);

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                m_quitRequested = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (!event.key.repeat)
                    input.onKey(mapScancode(event.key.scancode), true);
                break;
            case SDL_EVENT_KEY_UP:
                input.onKey(mapScancode(event.key.scancode), false);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (const auto button = mapMouseButton(event.button.button))
                    input.onMouseButton(*button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                input.onMouseMove({event.motion.x, event.motion.y}, {event.motion.xrel, event.motion.yrel});
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                input.onMouseWheel(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y);
                break;
            default:
                break;
            }
        }
    }

    void SdlPlatform::setWindowTitle(const std::string &title)
    {
        if (m_window)
            SDL_SetWindowTitle(m_window, title.c_str());
    }

} // namespace olam
