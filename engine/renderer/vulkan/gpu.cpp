#include "gpu.h"

#include <set>
#include "renderer.h"
#include "engine/config.h"

namespace lotus
{
    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
    };

    GPU::GPU(vk::Instance _instance, vk::SurfaceKHR _surface, Config* _config, const std::vector<const char*>& layers) : instance(_instance), surface(_surface), config(_config)
    {
        createPhysicalDevice();
        createDevice(layers);

        memory_manager = std::make_unique<MemoryManager>(physical_device, *device, instance);
    }

    void GPU::createPhysicalDevice()
    {
        auto physical_devices = instance.enumeratePhysicalDevices();

        auto physical_device_pointer = std::find_if(physical_devices.begin(), physical_devices.end(), [this](auto& device) {
            auto [graphics, present, compute] = getQueueFamilies(device);
            auto extensions_supported = extensionsSupported(device);
            auto formats = device.getSurfaceFormatsKHR(surface);
            auto present_modes = device.getSurfacePresentModesKHR(surface);
            auto supported_features = device.getFeatures();
            return graphics && present && compute && extensions_supported && !formats.empty() && !present_modes.empty() && supported_features.samplerAnisotropy;
        });

        if (physical_device_pointer == physical_devices.end())
        {
            throw std::runtime_error("Unable to find a suitable Vulkan GPU");
        }

        physical_device = *physical_device_pointer;

        ray_tracing_properties.maxRayRecursionDepth = 0;
        ray_tracing_properties.shaderGroupHandleSize = 0;
        properties.pNext = &ray_tracing_properties;
        physical_device.getProperties2(&properties);
    }

    void GPU::createDevice(const std::vector<const char*>& layers)
    {
        auto [graphics, present, compute] = getQueueFamilies(physical_device);
        graphics_queue_index = graphics.value();
        present_queue_index = present.value();
        compute_queue_index = compute.value();

        //deduplicate queues
        std::set<uint32_t> queues = { graphics_queue_index, present_queue_index, compute_queue_index };

        std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
        float queue_priority = 0.f;
        float queue_priority_compute[2] = { 0.f, 1.f };

        for (auto queue : queues)
        {
            vk::DeviceQueueCreateInfo create_info;
            create_info.queueFamilyIndex = queue;
            if (queue == compute_queue_index)
            {
                create_info.pQueuePriorities = queue_priority_compute;
                create_info.queueCount = 2;
            }
            else
            {
                create_info.pQueuePriorities = &queue_priority;
                create_info.queueCount = 1;
            }
            queue_create_infos.push_back(create_info);
        }

        vk::PhysicalDeviceFeatures physical_device_features;
        physical_device_features.samplerAnisotropy = true;
        physical_device_features.depthClamp = true;
        physical_device_features.independentBlend = true;
        physical_device_features.shaderInt64 = true;
        physical_device_features.shaderStorageImageWriteWithoutFormat = true;

        vk::PhysicalDeviceVulkan12Features vk_12_features;

        vk_12_features.uniformBufferStandardLayout = true;
        vk_12_features.bufferDeviceAddress = true;
        vk_12_features.descriptorBindingPartiallyBound = true;
        vk_12_features.descriptorIndexing = true;

        //this is part of the 1.2 spec but not in Vulkan12Features...?
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR as_features;
        as_features.accelerationStructure = true;
        as_features.descriptorBindingAccelerationStructureUpdateAfterBind = true;
        //some day nvidia will support this
        //as_features.accelerationStructureHostCommands = true;

        vk_12_features.pNext = &as_features;

        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rt_features;
        rt_features.rayTracingPipeline = true;
        rt_features.rayTracingPipelineTraceRaysIndirect = true;

        as_features.pNext = &rt_features;

        vk::PhysicalDeviceShaderClockFeaturesKHR clock_features;
        clock_features.shaderSubgroupClock = true;

        rt_features.pNext = &clock_features;

        std::vector<const char*> device_extensions2 = device_extensions;
        if (config->renderer.RaytraceEnabled())
        {
            device_extensions2.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            device_extensions2.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            device_extensions2.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            device_extensions2.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
            device_extensions2.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
            device_extensions2.push_back(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);
        }

        vk::DeviceCreateInfo device_create_info;
        device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create_info.pQueueCreateInfos = queue_create_infos.data();
        device_create_info.pEnabledFeatures = &physical_device_features;
        device_create_info.pNext = &vk_12_features;
        device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions2.size());
        device_create_info.ppEnabledExtensionNames = device_extensions2.data();

        if (!layers.empty()) {
            device_create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
            device_create_info.ppEnabledLayerNames = layers.data();
        } else {
            device_create_info.enabledLayerCount = 0;
        }
        device = physical_device.createDeviceUnique(device_create_info, nullptr);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

        graphics_queue = device->getQueue(graphics_queue_index, 0);
        present_queue = device->getQueue(present_queue_index, 0);
        compute_queue = device->getQueue(compute_queue_index, 0);
    }

    std::tuple<std::optional<uint32_t>, std::optional<std::uint32_t>, std::optional<std::uint32_t>> GPU::getQueueFamilies(vk::PhysicalDevice device) const
    {
        auto queue_families = device.getQueueFamilyProperties();
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;
        std::optional<uint32_t> compute;
        std::optional<uint32_t> compute_dedicated;

        for (size_t i = 0; i < queue_families.size(); ++i)
        {
            auto& family = queue_families[i];
            if (family.queueFlags & vk::QueueFlagBits::eGraphics && family.queueCount > 0)
            {
                graphics = static_cast<uint32_t>(i);
            }

            if (family.queueFlags & vk::QueueFlagBits::eCompute && family.queueCount > 0)
            {
                compute = static_cast<uint32_t>(i);
            }

            if (family.queueFlags & vk::QueueFlagBits::eCompute && (family.queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlags{} && family.queueCount > 0)
            {
                compute_dedicated = static_cast<uint32_t>(i);
            }

            if (device.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface) && family.queueCount > 0 && !present)
            {
                present = static_cast<uint32_t>(i);
            }
        }
        if (compute_dedicated)
            compute = compute_dedicated;
        return { graphics, present, compute };
    }

    bool GPU::extensionsSupported(vk::PhysicalDevice device)
    {
        auto supported_extensions = device.enumerateDeviceExtensionProperties(nullptr);

        std::set<std::string> requested_extensions{ device_extensions.begin(), device_extensions.end() };

        for (const auto& supported_extension : supported_extensions)
        {
            requested_extensions.erase(supported_extension.extensionName);
        }
        return requested_extensions.empty();
    }

    vk::Format GPU::getDepthFormat() const
    {
        for (vk::Format format : {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint})
        {
            vk::FormatProperties props = physical_device.getFormatProperties(format);

            if ((props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) == vk::FormatFeatureFlagBits::eDepthStencilAttachment)
            {
                return format;
            }
        }
        throw std::runtime_error("Unable to find supported depth format");
    }

    vk::UniqueCommandPool GPU::createCommandPool(QueueType type)
    {
        vk::CommandPoolCreateInfo pool_info = {};
        switch (type)
        {
        case QueueType::Graphics:
            pool_info.queueFamilyIndex = graphics_queue_index;
            break;
        case QueueType::Present:
            pool_info.queueFamilyIndex = present_queue_index;
            break;
        case QueueType::Compute:
            pool_info.queueFamilyIndex = compute_queue_index;
            break;
        }
        return device->createCommandPoolUnique(pool_info);
    }
}
