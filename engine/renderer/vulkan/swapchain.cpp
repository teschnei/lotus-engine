#include "swapchain.h"
#include "engine/config.h"
#include "gpu.h"
#include "window.h"

namespace lotus
{
    Swapchain::Swapchain(Config* _config, GPU* _gpu, Window* _window, vk::SurfaceKHR _surface) :
        config(_config), gpu(_gpu), window(_window), surface(_surface)
    {
        createSwapchain();
    }

    void Swapchain::recreateSwapchain(uint32_t image)
    {
        old_swapchain = std::move(swapchain);
        old_swapchain_image = image;
        swapchain.reset();
        images.clear();
        image_views.clear();
        createSwapchain();
    }

    void Swapchain::checkOldSwapchain(uint32_t image)
    {
        if (old_swapchain && image == old_swapchain_image)
        {
            old_swapchain.reset();
        }
    }

    void Swapchain::createSwapchain()
    {
        auto swap_chain_info = getSwapChainInfo(gpu->physical_device, surface);
        vk::SurfaceFormatKHR surface_format;
        if (swap_chain_info.formats.size() == 1 && swap_chain_info.formats[0].format == vk::Format::eUndefined)
        {
            surface_format = { vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear };
        }
        else
        {
            if (auto found_format = std::find_if(swap_chain_info.formats.begin(), swap_chain_info.formats.end(), [](const auto& format)
            {
                    return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            }); found_format != swap_chain_info.formats.end())
            {
                surface_format = *found_format;
            }
            else
            {
                surface_format = swap_chain_info.formats[0];
            }
        }
        //surface_format = swap_chain_info.formats[3];

        //test

        vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
        for (const auto& available_present_mode : swap_chain_info.present_modes)
        {
            if (available_present_mode == vk::PresentModeKHR::eMailbox)
            {
                present_mode = available_present_mode;
                break;
            }
            else if (available_present_mode == vk::PresentModeKHR::eImmediate)
            {
                present_mode = available_present_mode;
            }
        }

        vk::Extent2D swap_extent;
        if (swap_chain_info.capabilities.currentExtent.width == -1)
        {
            swap_extent = swap_chain_info.capabilities.currentExtent;
        }
        else
        {
            auto [width, height] = window->getWindowDimensions();

            swap_extent = vk::Extent2D
            {
                std::clamp(static_cast<uint32_t>(width), swap_chain_info.capabilities.minImageExtent.width, swap_chain_info.capabilities.maxImageExtent.width),
                std::clamp(static_cast<uint32_t>(height), swap_chain_info.capabilities.minImageExtent.height, swap_chain_info.capabilities.maxImageExtent.height)
            };
        }
        uint32_t image_count = swap_chain_info.capabilities.minImageCount + 1;
        if (swap_chain_info.capabilities.maxImageCount > 0)
            image_count = std::min(image_count, swap_chain_info.capabilities.maxImageCount);

        vk::SwapchainCreateInfoKHR swapchain_create_info;
        swapchain_create_info.surface = surface;
        swapchain_create_info.minImageCount = image_count;
        swapchain_create_info.imageFormat = surface_format.format;
        swapchain_create_info.imageColorSpace = surface_format.colorSpace;
        swapchain_create_info.imageExtent = swap_extent;
        swapchain_create_info.imageArrayLayers = 1;
        swapchain_create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

        if (old_swapchain)
        {
            swapchain_create_info.oldSwapchain = *old_swapchain;
        }

        uint32_t queueIndices[] = { gpu->graphics_queue_index, gpu->present_queue_index };
        if (gpu->graphics_queue_index != gpu->present_queue_index)
        {
            swapchain_create_info.imageSharingMode = vk::SharingMode::eConcurrent;
            swapchain_create_info.queueFamilyIndexCount = 2;
            swapchain_create_info.pQueueFamilyIndices = queueIndices;
        }
        else
        {
            swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
        }

        swapchain_create_info.preTransform = swap_chain_info.capabilities.currentTransform;
        swapchain_create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapchain_create_info.presentMode = present_mode;
        swapchain_create_info.clipped = VK_TRUE;

        image_format = surface_format.format;
        swapchain = gpu->device->createSwapchainKHRUnique(swapchain_create_info, nullptr);
        if (images.empty())
        {
            for (const auto& image : gpu->device->getSwapchainImagesKHR(*swapchain))
            {
                images.emplace_back(image);
            }
        }
        extent = swap_extent;

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = image_format;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        if (image_views.empty())
        {
            for (auto& swapchain_image : images)
            {
                image_view_info.image = swapchain_image;
                image_views.push_back(gpu->device->createImageViewUnique(image_view_info, nullptr));
            }
        }
    }

    Swapchain::swapChainInfo Swapchain::getSwapChainInfo(vk::PhysicalDevice device, vk::SurfaceKHR surface)
    {
        return {device.getSurfaceCapabilitiesKHR(surface),
            device.getSurfaceFormatsKHR(surface),
            device.getSurfacePresentModesKHR(surface)
        };
    }
}
