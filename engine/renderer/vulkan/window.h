#pragma once

#include <engine/renderer/sdl_inc.h>
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

        std::vector<const char*> getRequiredExtensions() const;
        std::pair<int, int> getWindowDimensions() const;
    private:
        Settings* settings;
        Config* config;
    };
}
