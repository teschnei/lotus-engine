#pragma once

#include <tuple>
#include <optional>
#include "vulkan_inc.h"
#include "engine/renderer/memory.h"

namespace lotus
{
    class Renderer;
    class Config;

    class GPU
    {
    public:
        GPU(vk::Instance instance, vk::SurfaceKHR surface, Config* config, const std::vector<const char*>& layers);

        vk::PhysicalDevice physical_device;
        vk::PhysicalDeviceProperties2 properties;
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_properties;
        vk::UniqueHandle<vk::Device, vk::DispatchLoaderDynamic> device;
        vk::Queue graphics_queue;
        vk::Queue present_queue;
        vk::Queue compute_queue;
        uint32_t graphics_queue_index;
        uint32_t present_queue_index;
        uint32_t compute_queue_index;
        std::unique_ptr<MemoryManager> memory_manager;

        vk::Format getDepthFormat() const;

        enum class QueueType
        {
            Graphics,
            Present,
            Compute
        };
        vk::UniqueCommandPool createCommandPool(QueueType type);
    private:
        vk::Instance instance{ nullptr };
        vk::SurfaceKHR surface{ nullptr };
        Config* config{ nullptr };

        void createPhysicalDevice();
        void createDevice(const std::vector<const char*>& layers);
        std::tuple<std::optional<uint32_t>, std::optional<std::uint32_t>, std::optional<uint32_t>> getQueueFamilies(vk::PhysicalDevice device) const;
        bool extensionsSupported(vk::PhysicalDevice device);
    };
}
