#pragma once
#include <cstdint>
#include <vector>

import vulkan_hpp;

namespace lotus
{
class Config;
class GPU;
class Window;

class Swapchain
{
public:
    Swapchain(Config* config, GPU* gpu, Window* window, vk::SurfaceKHR surface);

    void recreateSwapchain(uint32_t image);
    void checkOldSwapchain(uint32_t image);

    vk::UniqueSwapchainKHR swapchain;
    vk::Extent2D extent{};
    vk::Format image_format{};
    std::vector<vk::Image> images;

private:
    void createSwapchain();

    struct swapChainInfo
    {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> present_modes;
    };

    static swapChainInfo getSwapChainInfo(vk::PhysicalDevice device, vk::SurfaceKHR surface);

    Config* config;
    GPU* gpu;
    Window* window;
    vk::SurfaceKHR surface;
    vk::UniqueSwapchainKHR old_swapchain;
    uint32_t old_swapchain_image{0};
};
} // namespace lotus
