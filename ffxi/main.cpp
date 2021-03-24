#include <SDL2/SDL.h>

#include "ffxi.h"

int main(int argc, char* argv[]) {

    lotus::Settings settings;
    settings.app_name = "core-test";
    settings.app_version = VK_MAKE_VERSION(1, 0, 0);
    FFXIGame game{settings};

    game.run();
    return EXIT_SUCCESS;
}
