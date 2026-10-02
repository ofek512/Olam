#include "app/Application.h"
#include "app/CommandLine.h"
#include "core/logging/Log.h"

#include <SDL3/SDL_main.h>

int main(int argc, char *argv[])
{
    const olam::CommandLineOptions options = olam::parseCommandLine(argc, argv);
    for (const auto &error : options.errors)
        olam::logging::error(olam::LogCategory::Core, "Command line: {}", error);

    olam::ApplicationConfig config;
    config.seed = options.seed;
    if (options.worldWidth && options.worldHeight)
    {
        olam::WorldConfig world = config.world;
        world.width = *options.worldWidth;
        world.height = *options.worldHeight;
        if (const auto error = olam::validateWorldConfig(world))
            olam::logging::error(olam::LogCategory::Core, "Ignoring --size: {}", *error);
        else
            config.world = world;
    }

    olam::Application app(config);
    if (!app.initialize())
    {
        app.shutdown();
        return 1;
    }

    app.run();
    app.shutdown();
    return 0;
}
