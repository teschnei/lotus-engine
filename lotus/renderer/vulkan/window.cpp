module;

#include "lotus/renderer/sdl_inc.h"
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_hpp_macros.hpp>

module lotus;

import :renderer.vulkan.window;

import :core.config;
import :core.engine;
import :renderer.vulkan.gpu;
import vulkan_hpp;

namespace lotus
{
Window::Window(Settings* _settings, Config* _config) : settings(_settings), config(_config)
{
    window =
        SDL_CreateWindow(settings->app_name.c_str(), config->renderer.screen_width, config->renderer.screen_height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
}

vk::UniqueSurfaceKHR Window::createSurface(vk::Instance instance)
{
    VkSurfaceKHR vksurface;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &vksurface))
    {
        throw std::runtime_error("Unable to create SDL Vulkan surface");
    }
    return vk::UniqueSurfaceKHR(vksurface);
}

std::vector<const char*> Window::getRequiredExtensions() const
{
    uint32_t extensionCount = 0;

    auto sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    std::vector<const char*> extensions(extensionCount);

    SDL_memcpy(extensions.data(), sdl_extensions, extensionCount * sizeof(const char*));

    return extensions;
}

std::pair<int, int> Window::getWindowDimensions() const
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    return {width, height};
}

bool Window::isMinimized() const
{
    auto flags = SDL_GetWindowFlags(window);
    return (flags & SDL_WINDOW_MINIMIZED) > 0;
}
} // namespace lotus
