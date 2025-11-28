module;

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <tuple>
#include <vector>
#include <vulkan/vulkan_hpp_macros.hpp>

export module lotus:renderer.vulkan.gpu;

import :core.config;
import :renderer.memory;
import vulkan_hpp;

export namespace lotus
{
class GPU
{
public:
    GPU(vk::Instance instance, vk::SurfaceKHR surface, Config* config);

    vk::PhysicalDevice physical_device;
    vk::PhysicalDeviceProperties2 properties;
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_properties;
    vk::PhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties;
    vk::UniqueHandle<vk::Device, vk::DispatchLoaderDynamic> device;
    vk::Queue graphics_queue;
    vk::Queue present_queue;
    vk::Queue async_compute_queue;
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
    vk::UniqueCommandPool createCommandPool(QueueType type, vk::CommandPoolCreateFlags flags);

private:
    vk::Instance instance{nullptr};
    vk::SurfaceKHR surface{nullptr};
    Config* config{nullptr};

    void createPhysicalDevice();
    void createDevice();
    std::tuple<std::optional<uint32_t>, std::optional<std::uint32_t>, std::optional<uint32_t>> getQueueFamilies(vk::PhysicalDevice device) const;
    bool extensionsSupported(vk::PhysicalDevice device);
};

const std::vector<const char*> device_extensions = {vk::KHRSwapchainExtensionName};

GPU::GPU(vk::Instance _instance, vk::SurfaceKHR _surface, Config* _config) : instance(_instance), surface(_surface), config(_config)
{
    createPhysicalDevice();
    createDevice();

    memory_manager = std::make_unique<MemoryManager>(physical_device, *device, instance);
}

void GPU::createPhysicalDevice()
{
    auto physical_devices = instance.enumeratePhysicalDevices();

    auto physical_device_pointer = std::find_if(physical_devices.begin(), physical_devices.end(),
                                                [this](auto& device)
                                                {
                                                    auto [graphics, present, compute] = getQueueFamilies(device);
                                                    auto extensions_supported = extensionsSupported(device);
                                                    auto formats = device.getSurfaceFormatsKHR(surface);
                                                    auto present_modes = device.getSurfacePresentModesKHR(surface);
                                                    auto supported_features = device.getFeatures();
                                                    auto properties = device.getProperties2();
                                                    return graphics && present && compute && extensions_supported && !formats.empty() &&
                                                           !present_modes.empty() && supported_features.samplerAnisotropy &&
                                                           properties.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
                                                });

    if (physical_device_pointer == physical_devices.end())
    {
        physical_device_pointer = std::find_if(physical_devices.begin(), physical_devices.end(),
                                               [this](auto& device)
                                               {
                                                   auto [graphics, present, compute] = getQueueFamilies(device);
                                                   auto extensions_supported = extensionsSupported(device);
                                                   auto formats = device.getSurfaceFormatsKHR(surface);
                                                   auto present_modes = device.getSurfacePresentModesKHR(surface);
                                                   auto supported_features = device.getFeatures();
                                                   return graphics && present && compute && extensions_supported && !formats.empty() &&
                                                          !present_modes.empty() && supported_features.samplerAnisotropy;
                                               });
    }

    if (physical_device_pointer == physical_devices.end())
    {
        throw std::runtime_error("Unable to find a suitable Vulkan GPU");
    }

    physical_device = *physical_device_pointer;

    ray_tracing_properties.maxRayRecursionDepth = 0;
    ray_tracing_properties.shaderGroupHandleSize = 0;
    properties.pNext = &ray_tracing_properties;
    ray_tracing_properties.pNext = &acceleration_structure_properties;
    physical_device.getProperties2(&properties);
}

void GPU::createDevice()
{
    auto [graphics, present, compute] = getQueueFamilies(physical_device);
    graphics_queue_index = graphics.value();
    present_queue_index = present.value();
    compute_queue_index = compute.value();

    // deduplicate queues
    std::set<uint32_t> queues = {graphics_queue_index, present_queue_index, compute_queue_index};

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    float queue_priority = 0.f;
    float queue_priority_compute[2] = {0.f, 1.f};

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

    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rt_features{.rayTracingPipeline = config->renderer.RaytraceEnabled(),
                                                                .rayTracingPipelineTraceRaysIndirect = config->renderer.RaytraceEnabled()};

    vk::PhysicalDeviceAccelerationStructureFeaturesKHR as_features{.pNext = &rt_features,
                                                                   .accelerationStructure = config->renderer.RaytraceEnabled(),
                                                                   .descriptorBindingAccelerationStructureUpdateAfterBind = config->renderer.RaytraceEnabled()};

    vk::PhysicalDeviceShaderClockFeaturesKHR clock_features{.pNext = &as_features, .shaderSubgroupClock = true};

    vk::PhysicalDeviceVulkan14Features vk_14_features{.pNext = &clock_features, .pushDescriptor = true};

    vk::PhysicalDeviceVulkan13Features vk_13_features{
        .pNext = &vk_14_features,
        .synchronization2 = true,
        .dynamicRendering = true,
        .maintenance4 = true,
    };

    vk::PhysicalDeviceVulkan12Features vk_12_features{.pNext = &vk_13_features,
                                                      .shaderInt8 = true,
                                                      .descriptorIndexing = true,
                                                      .shaderSampledImageArrayNonUniformIndexing = true,
                                                      .descriptorBindingUniformBufferUpdateAfterBind = true,
                                                      .descriptorBindingSampledImageUpdateAfterBind = true,
                                                      .descriptorBindingStorageBufferUpdateAfterBind = true,
                                                      .descriptorBindingPartiallyBound = true,
                                                      .runtimeDescriptorArray = true,
                                                      .scalarBlockLayout = true,
                                                      .uniformBufferStandardLayout = true,
                                                      .timelineSemaphore = true,
                                                      .bufferDeviceAddress = true};

    vk::PhysicalDeviceVulkan11Features vk_11_features{.pNext = &vk_12_features, .storageBuffer16BitAccess = true, .shaderDrawParameters = true};

    vk::PhysicalDeviceFeatures physical_device_features{.independentBlend = true,
                                                        .depthClamp = true,
                                                        .samplerAnisotropy = true,
                                                        .shaderStorageImageWriteWithoutFormat = true,
                                                        .shaderInt64 = true,
                                                        .shaderInt16 = true};

    std::vector<const char*> device_extensions2 = device_extensions;
    if (config->renderer.RaytraceEnabled())
    {
        device_extensions2.push_back(vk::KHRRayTracingPipelineExtensionName);
        device_extensions2.push_back(vk::KHRAccelerationStructureExtensionName);
        device_extensions2.push_back(vk::KHRDeferredHostOperationsExtensionName);
        device_extensions2.push_back(vk::KHRPipelineLibraryExtensionName);
        device_extensions2.push_back(vk::KHRShaderClockExtensionName);
#ifdef WIN32
        device_extensions2.push_back(vk::KHRExternalMemoryWin32ExtensionName);
#else
        device_extensions2.push_back(vk::KHRExternalMemoryFdExtensionName);
        device_extensions2.push_back(vk::EXTExternalMemoryDmaBufExtensionName);
#endif
    }

    vk::DeviceCreateInfo device_create_info{.pNext = &vk_11_features,
                                            .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
                                            .pQueueCreateInfos = queue_create_infos.data(),
                                            .enabledExtensionCount = static_cast<uint32_t>(device_extensions2.size()),
                                            .ppEnabledExtensionNames = device_extensions2.data(),
                                            .pEnabledFeatures = &physical_device_features};

    device = physical_device.createDeviceUnique(device_create_info, nullptr);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    graphics_queue = device->getQueue(graphics_queue_index, 0);
    present_queue = device->getQueue(present_queue_index, 0);
    async_compute_queue = device->getQueue(compute_queue_index, 0);
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
    return {graphics, present, compute};
}

bool GPU::extensionsSupported(vk::PhysicalDevice device)
{
    auto supported_extensions = device.enumerateDeviceExtensionProperties(nullptr);

    std::set<std::string> requested_extensions{device_extensions.begin(), device_extensions.end()};

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

vk::UniqueCommandPool GPU::createCommandPool(QueueType type, vk::CommandPoolCreateFlags flags)
{
    vk::CommandPoolCreateInfo pool_info{.flags = flags};
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
} // namespace lotus
