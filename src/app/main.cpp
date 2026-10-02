#include "app/Application.h"

#include <SDL3/SDL_main.h>

int main(int, char *[])
{
    olam::Application app;
    if (!app.initialize())
    {
        app.shutdown();
        return 1;
    }

    app.run();
    app.shutdown();
    return 0;
}
