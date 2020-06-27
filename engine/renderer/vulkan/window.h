#pragma once

#include <SDL2/SDL.h>
#include "vulkan_inc.h"

namespace lotus
{
    class GPU;
    class Config;
    struct Settings;

    class Window
    {
    public:
        Window(Settings* settings, Config* config);

        vk::UniqueSurfaceKHR createSurface(vk::Instance instance);

        SDL_Window* window {nullptr};
        vk::UniqueSurfaceKHR surface;

        std::vector<const char*> getRequiredExtensions();
    private:
        Settings* settings;
        Config* config;
    };
}
