#include "renderer.h"
#include <SDL2/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <fstream>
#include <iostream>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/config.h"

constexpr size_t shadowmap_dimension = 2048;

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace lotus
{

    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_monitor"
    };
    #ifdef NDEBUG
    const bool enableValidationLayers = false;
    #else
    const bool enableValidationLayers = true;
    #endif

    const std::vector<const char*> instance_extensions = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };

    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
    };

    VkResult CreateDebugUtilsMessengerEXT(vk::Instance instance, const vk::DebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, (VkDebugUtilsMessengerCreateInfoEXT*)pCreateInfo, pAllocator, pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(vk::Instance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debug_messenger, pAllocator);
        }
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    Renderer::Renderer(Engine* _engine) : render_mode{RenderMode::Hybrid}, engine(_engine)
    {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow(engine->settings.app_name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, engine->config->renderer.screen_width, engine->config->renderer.screen_height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        createInstance(engine->settings.app_name, engine->settings.app_version);
        if (!SDL_Vulkan_CreateSurface(window, *instance, reinterpret_cast<VkSurfaceKHR*>(&surface)))
        {
            throw std::runtime_error("Unable to create SDL Vulkan surface");
        }
        createPhysicalDevice();
        createDevice();

        memory_manager = std::make_unique<MemoryManager>(physical_device, *device, dispatch);

        createSwapchain();
        createRenderpasses();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createDepthImage();
        createFramebuffers();
        createSyncs();
        createCommandPool();
        createShadowmapResources();
        createGBufferResources();
        createQuad();
        createRayTracingResources();
        createAnimationResources();

        render_commandbuffers.resize(getImageCount());
        raytracer = std::make_unique<Raytracer>(engine);
    }

    Renderer::~Renderer()
    {
        device->waitIdle(dispatch);
        if (mesh_info_buffer)
            mesh_info_buffer->unmap();
    }

    void Renderer::generateCommandBuffers()
    {
        createDeferredCommandBuffer();
    }

    bool Renderer::RaytraceEnabled()
    {
        return render_mode == RenderMode::Hybrid || render_mode == RenderMode::Raytrace;
    }

    bool Renderer::RasterizationEnabled()
    {
        return render_mode == RenderMode::Rasterization || render_mode == RenderMode::Hybrid;
    }

    void Renderer::createRayTracingResources()
    {
        if (RaytraceEnabled())
        {
            if (!shader_binding_table)
            {
                constexpr uint32_t shader_raygencount = 1;
                constexpr uint32_t shader_misscount = 2;
                constexpr uint32_t shader_nonhitcount = shader_raygencount + shader_misscount;
                constexpr uint32_t shader_hitcount = shaders_per_group * 6;
                vk::DeviceSize shader_handle_size = ray_tracing_properties.shaderGroupHandleSize;
                vk::DeviceSize nonhit_shader_stride = shader_handle_size;
                vk::DeviceSize hit_shader_stride = nonhit_shader_stride;
                vk::DeviceSize shader_offset_raygen = 0;
                vk::DeviceSize shader_offset_miss = (((nonhit_shader_stride * shader_raygencount) / engine->renderer.ray_tracing_properties.shaderGroupBaseAlignment) + 1) * engine->renderer.ray_tracing_properties.shaderGroupBaseAlignment;
                vk::DeviceSize shader_offset_hit = shader_offset_miss + (((nonhit_shader_stride * shader_misscount) / engine->renderer.ray_tracing_properties.shaderGroupBaseAlignment) + 1) * engine->renderer.ray_tracing_properties.shaderGroupBaseAlignment;
                vk::DeviceSize sbt_size = (hit_shader_stride * shader_hitcount) + shader_offset_hit;
                shader_binding_table = engine->renderer.memory_manager->GetBuffer(sbt_size, vk::BufferUsageFlagBits::eRayTracingKHR, vk::MemoryPropertyFlagBits::eHostVisible);

                uint8_t* shader_mapped = static_cast<uint8_t*>(shader_binding_table->map(0, sbt_size, {}));

                std::vector<uint8_t> shader_handle_storage((shader_hitcount + shader_nonhitcount) * shader_handle_size);
                device->getRayTracingShaderGroupHandlesKHR(*rtx_pipeline, 0, shader_nonhitcount + shader_hitcount, shader_handle_storage.size(), shader_handle_storage.data(), dispatch);
                for (uint32_t i = 0; i < shader_raygencount; ++i)
                {
                    memcpy(shader_mapped + shader_offset_raygen + (i * nonhit_shader_stride), shader_handle_storage.data() + (i * shader_handle_size), shader_handle_size);
                }
                for (uint32_t i = 0; i < shader_misscount; ++i)
                {
                    memcpy(shader_mapped + shader_offset_miss + (i * nonhit_shader_stride), shader_handle_storage.data() + (shader_handle_size * shader_raygencount) + (i * shader_handle_size), shader_handle_size);
                }
                for (uint32_t i = 0; i < shader_hitcount; ++i)
                {
                    memcpy(shader_mapped + shader_offset_hit + (i * hit_shader_stride), shader_handle_storage.data() + (shader_handle_size * shader_nonhitcount) + (i * shader_handle_size), shader_handle_size);
                }
                shader_binding_table->unmap();

                raygenSBT = vk::StridedBufferRegionKHR{ shader_binding_table->buffer, shader_offset_raygen, nonhit_shader_stride, nonhit_shader_stride * shader_raygencount };
                missSBT = vk::StridedBufferRegionKHR{ shader_binding_table->buffer, shader_offset_miss, nonhit_shader_stride, nonhit_shader_stride * shader_misscount };
                hitSBT = vk::StridedBufferRegionKHR{ shader_binding_table->buffer, shader_offset_hit, hit_shader_stride, hit_shader_stride * shader_hitcount };

                std::vector<vk::DescriptorPoolSize> pool_sizes_const;
                pool_sizes_const.emplace_back(vk::DescriptorType::eAccelerationStructureKHR, 1);
                pool_sizes_const.emplace_back(vk::DescriptorType::eStorageBuffer, max_acceleration_binding_index);
                pool_sizes_const.emplace_back(vk::DescriptorType::eStorageBuffer, max_acceleration_binding_index);
                pool_sizes_const.emplace_back(vk::DescriptorType::eCombinedImageSampler, max_acceleration_binding_index);
                pool_sizes_const.emplace_back(vk::DescriptorType::eUniformBuffer, 1);

                vk::DescriptorPoolCreateInfo pool_ci;
                pool_ci.maxSets = 3;
                pool_ci.poolSizeCount = static_cast<uint32_t>(pool_sizes_const.size());
                pool_ci.pPoolSizes = pool_sizes_const.data();
                pool_ci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

                rtx_descriptor_pool_const = device->createDescriptorPoolUnique(pool_ci, nullptr, dispatch);

                std::array<vk::DescriptorSetLayout, 3> layouts = { *rtx_descriptor_layout_const, *rtx_descriptor_layout_const, *rtx_descriptor_layout_const };

                vk::DescriptorSetAllocateInfo set_ci;
                set_ci.descriptorPool = *rtx_descriptor_pool_const;
                set_ci.descriptorSetCount = 3;
                set_ci.pSetLayouts = layouts.data();
                rtx_descriptor_sets_const = device->allocateDescriptorSetsUnique<std::allocator<vk::UniqueHandle<vk::DescriptorSet, vk::DispatchLoaderDynamic>>>(set_ci, dispatch);
            }
        }
    }

    void Renderer::createInstance(const std::string& app_name, uint32_t app_version)
    {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        vk::ApplicationInfo appInfo = {};
        appInfo.pApplicationName = app_name.c_str();
        appInfo.applicationVersion = app_version;
        appInfo.pEngineName = "lotus";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_1;

        vk::InstanceCreateInfo createInfo = {};
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            createInfo.ppEnabledLayerNames = validation_layers.data();

            debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
            debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
            debugCreateInfo.pfnUserCallback = debugCallback;
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)& debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        instance = vk::createInstanceUnique(createInfo, nullptr, dispatch_static);

    }

    void Renderer::createPhysicalDevice()
    {
        auto physical_devices = instance->enumeratePhysicalDevices(dispatch_static);

        physical_device = *std::find_if(physical_devices.begin(), physical_devices.end(), [this](auto& device) {
            auto [graphics, present, compute] = getQueueFamilies(device);
            auto extensions_supported = extensionsSupported(device);
            auto swap_chain_info = getSwapChainInfo(device);
            auto supported_features = device.getFeatures(dispatch_static);
            return graphics && present && compute && extensions_supported && !swap_chain_info.formats.empty() && !swap_chain_info.present_modes.empty() && supported_features.samplerAnisotropy;
        });

        if (!physical_device)
        {
            throw std::runtime_error("Unable to find a suitable Vulkan GPU");
        }

        ray_tracing_properties.maxRecursionDepth = 0;
        ray_tracing_properties.shaderGroupHandleSize = 0;
        properties.pNext = &ray_tracing_properties;
        physical_device.getProperties2(&properties, dispatch_static);
    }

    void Renderer::createDevice()
    {
        auto [graphics_queue_idx, present_queue_idx, compute_queue_idx] = getQueueFamilies(physical_device);
        //deduplicate queues
        std::set<uint32_t> queues = { graphics_queue_idx.value(), present_queue_idx.value(), compute_queue_idx.value() };

        std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
        float queue_priority = 0.f;
        float queue_priority_compute[2] = { 0.f, 1.f };

        for (auto queue : queues)
        {
            vk::DeviceQueueCreateInfo create_info;
            create_info.queueFamilyIndex = queue;
            if (queue == compute_queue_idx.value())
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

        vk::PhysicalDeviceUniformBufferStandardLayoutFeatures buffer_layout_features;
        buffer_layout_features.uniformBufferStandardLayout = true;

        vk::PhysicalDeviceRayTracingFeaturesKHR rt_features;
        rt_features.rayTracing = true;

        buffer_layout_features.pNext = &rt_features;

        vk::PhysicalDeviceBufferDeviceAddressFeatures buffer_address_features;
        buffer_address_features.bufferDeviceAddress = true;

        rt_features.pNext = &buffer_address_features;

        vk::PhysicalDeviceDescriptorIndexingFeatures indexing_features;
        indexing_features.descriptorBindingPartiallyBound = true;

        buffer_address_features.pNext = &indexing_features;

        std::vector<const char*> device_extensions2 = device_extensions;
        if (RaytraceEnabled())
        {
            device_extensions2.push_back(VK_KHR_RAY_TRACING_EXTENSION_NAME);
            device_extensions2.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
            device_extensions2.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
            device_extensions2.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            device_extensions2.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
            device_extensions2.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        }

        vk::DeviceCreateInfo device_create_info;
        device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create_info.pQueueCreateInfos = queue_create_infos.data();
        device_create_info.pEnabledFeatures = &physical_device_features;
        device_create_info.pNext = &buffer_layout_features;
        device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions2.size());
        device_create_info.ppEnabledExtensionNames = device_extensions2.data();

        if (enableValidationLayers) {
            device_create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            device_create_info.ppEnabledLayerNames = validation_layers.data();
        } else {
            device_create_info.enabledLayerCount = 0;
        }
        device = physical_device.createDeviceUnique(device_create_info, nullptr, dispatch_static);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance, *device);
        dispatch = VULKAN_HPP_DEFAULT_DISPATCHER;

        graphics_queue = device->getQueue(graphics_queue_idx.value(), 0);
        present_queue = device->getQueue(present_queue_idx.value(), 0);
        compute_queue = device->getQueue(compute_queue_idx.value(), 0);

        if (enableValidationLayers)
        {
            vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
            debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
            debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
            debugCreateInfo.pfnUserCallback = debugCallback;
            if (CreateDebugUtilsMessengerEXT(*instance, &debugCreateInfo, nullptr, &debug_messenger) != VK_SUCCESS) {
                throw std::runtime_error("failed to set up debug messenger!");
            }
        }
    }

    void Renderer::createSwapchain()
    {
        if (swapchain)
        {
            old_swapchain = std::move(swapchain);
            old_swapchain_image = getCurrentImage();
            swapchain.reset();
        }
        auto swap_chain_info = getSwapChainInfo(physical_device);
        vk::SurfaceFormatKHR surface_format;
        if (swap_chain_info.formats.size() == 1 && swap_chain_info.formats[0].format == vk::Format::eUndefined)
        {
            surface_format = { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
        }
        else
        {
            if (auto found_format = std::find_if(swap_chain_info.formats.begin(), swap_chain_info.formats.end(), [](const auto& format)
            {
                    return format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            }); found_format != swap_chain_info.formats.end())
            {
                surface_format = *found_format;
            }
            else
            {
                surface_format = swap_chain_info.formats[0];
            }
        }

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
            int width, height;
            SDL_GetWindowSize(window, &width, &height);

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
        if (RaytraceEnabled())
        {
            swapchain_create_info.imageUsage |= vk::ImageUsageFlagBits::eStorage;
        }

        if (old_swapchain)
        {
            swapchain_create_info.oldSwapchain = *old_swapchain;
        }

        auto [graphics, present, compute] = getQueueFamilies(physical_device);
        uint32_t queueIndices[] = { graphics.value(), present.value() };
        if (graphics.value() != present.value())
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

        swapchain_image_format = surface_format.format;
        swapchain = device->createSwapchainKHRUnique(swapchain_create_info, nullptr, dispatch);
        if (swapchain_images.empty())
        {
            for (const auto& image : device->getSwapchainImagesKHR(*swapchain, dispatch))
            {
                swapchain_images.emplace_back(image);
            }
        }
        swapchain_extent = swap_extent;

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = swapchain_image_format;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        if (swapchain_image_views.empty())
        {
            for (auto& swapchain_image : swapchain_images)
            {
                image_view_info.image = swapchain_image;
                swapchain_image_views.push_back(device->createImageViewUnique(image_view_info, nullptr, dispatch));
            }
        }
    }

    void Renderer::createRenderpasses()
    {
        {
            vk::AttachmentDescription color_attachment;
            color_attachment.format = swapchain_image_format;
            color_attachment.samples = vk::SampleCountFlagBits::e1;
            color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
            color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
            color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            color_attachment.initialLayout = vk::ImageLayout::eUndefined;
            color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

            vk::AttachmentDescription depth_attachment;
            depth_attachment.format = getDepthFormat();
            depth_attachment.samples = vk::SampleCountFlagBits::e1;
            depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
            depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
            depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
            depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            vk::AttachmentReference color_attachment_ref;
            color_attachment_ref.attachment = 0;
            color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference depth_attachment_ref;
            depth_attachment_ref.attachment = 1;
            depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            vk::SubpassDescription subpass = {};
            subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_attachment_ref;
            subpass.pDepthStencilAttachment = &depth_attachment_ref;

            vk::SubpassDependency dependency;
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

            std::array<vk::AttachmentDescription, 2> attachments = { color_attachment, depth_attachment };
            vk::RenderPassCreateInfo render_pass_info;
            render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            render_pass_info.pAttachments = attachments.data();
            render_pass_info.subpassCount = 1;
            render_pass_info.pSubpasses = &subpass;
            render_pass_info.dependencyCount = 1;
            render_pass_info.pDependencies = &dependency;

            render_pass = device->createRenderPassUnique(render_pass_info, nullptr, dispatch);

            if (render_mode == RenderMode::Rasterization)
            {
                depth_attachment.storeOp = vk::AttachmentStoreOp::eStore;
                depth_attachment_ref.attachment = 0;

                std::array<vk::AttachmentDescription, 1> shadow_attachments = { depth_attachment };
                subpass.colorAttachmentCount = 0;
                subpass.pColorAttachments = nullptr;
                render_pass_info.attachmentCount = static_cast<uint32_t>(shadow_attachments.size());
                render_pass_info.pAttachments = shadow_attachments.data();

                vk::SubpassDependency shadowmap_dep1;
                shadowmap_dep1.srcSubpass = VK_SUBPASS_EXTERNAL;
                shadowmap_dep1.dstSubpass = 0;
                shadowmap_dep1.srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;
                shadowmap_dep1.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
                shadowmap_dep1.srcAccessMask = vk::AccessFlagBits::eShaderRead;
                shadowmap_dep1.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                shadowmap_dep1.dependencyFlags = vk::DependencyFlagBits::eByRegion;

                vk::SubpassDependency shadowmap_dep2;
                shadowmap_dep2.srcSubpass = 0;
                shadowmap_dep2.dstSubpass = VK_SUBPASS_EXTERNAL;
                shadowmap_dep2.srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests;
                shadowmap_dep2.dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
                shadowmap_dep2.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                shadowmap_dep2.dstAccessMask = vk::AccessFlagBits::eShaderRead;
                shadowmap_dep2.dependencyFlags = vk::DependencyFlagBits::eByRegion;

                std::array<vk::SubpassDependency, 2> shadowmap_subpass_deps = { shadowmap_dep1, shadowmap_dep2 };

                render_pass_info.dependencyCount = 2;
                render_pass_info.pDependencies = shadowmap_subpass_deps.data();

                shadowmap_subpass_deps[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
                shadowmap_subpass_deps[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                shadowmap_subpass_deps[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
                shadowmap_subpass_deps[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

                shadowmap_subpass_deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                shadowmap_subpass_deps[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
                shadowmap_subpass_deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
                shadowmap_subpass_deps[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;

                shadowmap_render_pass = device->createRenderPassUnique(render_pass_info, nullptr, dispatch);
            }
            vk::AttachmentDescription desc_pos;
            desc_pos.format = vk::Format::eR32G32B32A32Sfloat;
            desc_pos.samples = vk::SampleCountFlagBits::e1;
            desc_pos.loadOp = vk::AttachmentLoadOp::eClear;
            desc_pos.storeOp = vk::AttachmentStoreOp::eStore;
            desc_pos.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_pos.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_pos.initialLayout = vk::ImageLayout::eUndefined;
            desc_pos.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_normal;
            desc_normal.format = vk::Format::eR32G32B32A32Sfloat;
            desc_normal.samples = vk::SampleCountFlagBits::e1;
            desc_normal.loadOp = vk::AttachmentLoadOp::eClear;
            desc_normal.storeOp = vk::AttachmentStoreOp::eStore;
            desc_normal.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_normal.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_normal.initialLayout = vk::ImageLayout::eUndefined;
            desc_normal.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_albedo;
            desc_albedo.format = vk::Format::eR8G8B8A8Unorm;
            desc_albedo.samples = vk::SampleCountFlagBits::e1;
            desc_albedo.loadOp = vk::AttachmentLoadOp::eClear;
            desc_albedo.storeOp = vk::AttachmentStoreOp::eStore;
            desc_albedo.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_albedo.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_albedo.initialLayout = vk::ImageLayout::eUndefined;
            desc_albedo.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_particle;
            desc_particle.format = vk::Format::eR8G8B8A8Unorm;
            desc_particle.samples = vk::SampleCountFlagBits::e1;
            desc_particle.loadOp = vk::AttachmentLoadOp::eClear;
            desc_particle.storeOp = vk::AttachmentStoreOp::eStore;
            desc_particle.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_particle.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_particle.initialLayout = vk::ImageLayout::eUndefined;
            desc_particle.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_face_normal;
            desc_face_normal.format = vk::Format::eR32G32B32A32Sfloat;
            desc_face_normal.samples = vk::SampleCountFlagBits::e1;
            desc_face_normal.loadOp = vk::AttachmentLoadOp::eClear;
            desc_face_normal.storeOp = vk::AttachmentStoreOp::eStore;
            desc_face_normal.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_face_normal.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_face_normal.initialLayout = vk::ImageLayout::eUndefined;
            desc_face_normal.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentDescription desc_material;
            desc_material.format = vk::Format::eR16Uint;
            desc_material.samples = vk::SampleCountFlagBits::e1;
            desc_material.loadOp = vk::AttachmentLoadOp::eClear;
            desc_material.storeOp = vk::AttachmentStoreOp::eStore;
            desc_material.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            desc_material.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            desc_material.initialLayout = vk::ImageLayout::eUndefined;
            desc_material.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            std::vector<vk::AttachmentDescription> gbuffer_attachments = { desc_pos, desc_normal, desc_face_normal, desc_albedo, desc_particle, desc_material, depth_attachment };

            render_pass_info.attachmentCount = static_cast<uint32_t>(gbuffer_attachments.size());
            render_pass_info.pAttachments = gbuffer_attachments.data();

            vk::AttachmentReference gbuffer_pos_attachment_ref;
            gbuffer_pos_attachment_ref.attachment = 0;
            gbuffer_pos_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_normal_attachment_ref;
            gbuffer_normal_attachment_ref.attachment = 1;
            gbuffer_normal_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_face_normal_attachment_ref;
            gbuffer_face_normal_attachment_ref.attachment = 2;
            gbuffer_face_normal_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_albedo_attachment_ref;
            gbuffer_albedo_attachment_ref.attachment = 3;
            gbuffer_albedo_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_particle_attachment_ref;
            gbuffer_particle_attachment_ref.attachment = 4;
            gbuffer_particle_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_material_attachment_ref;
            gbuffer_material_attachment_ref.attachment = 5;
            gbuffer_material_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference gbuffer_depth_attachment_ref;
            gbuffer_depth_attachment_ref.attachment = 6;
            gbuffer_depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            std::vector<vk::AttachmentReference> gbuffer_color_attachment_refs = { gbuffer_pos_attachment_ref, gbuffer_normal_attachment_ref, gbuffer_face_normal_attachment_ref,
                gbuffer_albedo_attachment_ref, gbuffer_material_attachment_ref };

            vk::SubpassDescription subpass_deferred;
            subpass_deferred.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            subpass_deferred.colorAttachmentCount = static_cast<uint32_t>(gbuffer_color_attachment_refs.size());
            subpass_deferred.pColorAttachments = gbuffer_color_attachment_refs.data();
            subpass_deferred.pDepthStencilAttachment = &gbuffer_depth_attachment_ref;

            std::vector<vk::AttachmentReference> gbuffer_color_attachment_particle_refs = { gbuffer_particle_attachment_ref };
            std::vector<uint32_t> gbuffer_preserve_attachment_particle_refs = { gbuffer_pos_attachment_ref.attachment, gbuffer_normal_attachment_ref.attachment,
                gbuffer_face_normal_attachment_ref.attachment, gbuffer_albedo_attachment_ref.attachment, gbuffer_material_attachment_ref.attachment };
            vk::SubpassDescription subpass_particle;
            subpass_particle.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            subpass_particle.colorAttachmentCount = static_cast<uint32_t>(gbuffer_color_attachment_particle_refs.size());
            subpass_particle.pColorAttachments = gbuffer_color_attachment_particle_refs.data();
            subpass_particle.preserveAttachmentCount = gbuffer_preserve_attachment_particle_refs.size();
            subpass_particle.pPreserveAttachments = gbuffer_preserve_attachment_particle_refs.data();
            subpass_particle.pDepthStencilAttachment = &gbuffer_depth_attachment_ref;

            vk::SubpassDependency gbuffer_dependency_end;
            gbuffer_dependency_end.srcSubpass = 0;
            gbuffer_dependency_end.dstSubpass = VK_SUBPASS_EXTERNAL;
            gbuffer_dependency_end.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            gbuffer_dependency_end.dstStageMask = vk::PipelineStageFlagBits::eRayTracingShaderKHR;
            gbuffer_dependency_end.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            gbuffer_dependency_end.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            vk::SubpassDependency gbuffer_dependency_particles;
            gbuffer_dependency_particles.srcSubpass = 0;
            gbuffer_dependency_particles.dstSubpass = 1;
            gbuffer_dependency_particles.srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests;
            gbuffer_dependency_particles.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
            gbuffer_dependency_particles.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            gbuffer_dependency_particles.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead;

            std::vector<vk::SubpassDependency> dependencies = { dependency, gbuffer_dependency_end, gbuffer_dependency_particles };
            std::vector<vk::SubpassDescription> subpasses = { subpass_deferred, subpass_particle };

            render_pass_info.dependencyCount = dependencies.size();
            render_pass_info.pDependencies = dependencies.data();
            render_pass_info.subpassCount = subpasses.size();
            render_pass_info.pSubpasses = subpasses.data();

            gbuffer_render_pass = device->createRenderPassUnique(render_pass_info, nullptr, dispatch);
        }
        if (RaytraceEnabled())
        {
            vk::AttachmentDescription output_attachment;
            output_attachment.format = swapchain_image_format;
            output_attachment.samples = vk::SampleCountFlagBits::e1;
            output_attachment.loadOp = vk::AttachmentLoadOp::eClear;
            output_attachment.storeOp = vk::AttachmentStoreOp::eStore;
            output_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            output_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            output_attachment.initialLayout = vk::ImageLayout::eUndefined;
            output_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

            vk::AttachmentDescription depth_attachment;
            depth_attachment.format = getDepthFormat();
            depth_attachment.samples = vk::SampleCountFlagBits::e1;
            depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
            depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
            depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
            depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            vk::AttachmentReference deferred_output_attachment_ref;
            deferred_output_attachment_ref.attachment = 0;
            deferred_output_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

            vk::AttachmentReference deferred_depth_attachment_ref;
            deferred_depth_attachment_ref.attachment = 1;
            deferred_depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            std::vector<vk::AttachmentReference> deferred_color_attachment_refs = { deferred_output_attachment_ref };

            vk::SubpassDescription rtx_subpass;
            rtx_subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            rtx_subpass.colorAttachmentCount = static_cast<uint32_t>(deferred_color_attachment_refs.size());
            rtx_subpass.pColorAttachments = deferred_color_attachment_refs.data();
            rtx_subpass.pDepthStencilAttachment = &deferred_depth_attachment_ref;

            vk::SubpassDependency dependency_initial;
            dependency_initial.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency_initial.dstSubpass = 0;
            dependency_initial.srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
            dependency_initial.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            dependency_initial.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
            dependency_initial.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
            dependency_initial.dependencyFlags = vk::DependencyFlagBits::eByRegion;

            std::vector<vk::AttachmentDescription> gbuffer_attachments = { output_attachment, depth_attachment };
            std::vector<vk::SubpassDescription> subpasses = { rtx_subpass };
            std::vector<vk::SubpassDependency> subpass_dependencies = { dependency_initial };

            vk::RenderPassCreateInfo render_pass_info;
            render_pass_info.attachmentCount = static_cast<uint32_t>(gbuffer_attachments.size());
            render_pass_info.pAttachments = gbuffer_attachments.data();
            render_pass_info.subpassCount = static_cast<uint32_t>(subpasses.size());
            render_pass_info.pSubpasses = subpasses.data();
            render_pass_info.dependencyCount = static_cast<uint32_t>(subpass_dependencies.size());
            render_pass_info.pDependencies = subpass_dependencies.data();

            rtx_render_pass = device->createRenderPassUnique(render_pass_info, nullptr, dispatch);
        }
    }

    void Renderer::createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding camera_layout_binding;
        camera_layout_binding.binding = 0;
        camera_layout_binding.descriptorCount = 1;
        camera_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        camera_layout_binding.pImmutableSamplers = nullptr;
        camera_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutBinding sample_layout_binding;
        sample_layout_binding.binding = 1;
        sample_layout_binding.descriptorCount = 1;
        sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        sample_layout_binding.pImmutableSamplers = nullptr;
        sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding model_layout_binding;
        model_layout_binding.binding = 2;
        model_layout_binding.descriptorCount = 1;
        model_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        model_layout_binding.pImmutableSamplers = nullptr;
        model_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutBinding mesh_info_layout_binding;
        mesh_info_layout_binding.binding = 3;
        mesh_info_layout_binding.descriptorCount = 1;
        mesh_info_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        mesh_info_layout_binding.pImmutableSamplers = nullptr;
        mesh_info_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding mesh_info_index_layout_binding;
        mesh_info_index_layout_binding.binding = 4;
        mesh_info_index_layout_binding.descriptorCount = 1;
        mesh_info_index_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        mesh_info_index_layout_binding.pImmutableSamplers = nullptr;
        mesh_info_index_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        std::vector<vk::DescriptorSetLayoutBinding> static_bindings = { camera_layout_binding, model_layout_binding, sample_layout_binding, mesh_info_layout_binding, mesh_info_index_layout_binding };

        vk::DescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        layout_info.bindingCount = static_cast<uint32_t>(static_bindings.size());
        layout_info.pBindings = static_bindings.data();

        static_descriptor_set_layout = device->createDescriptorSetLayoutUnique(layout_info, nullptr, dispatch);

        if (render_mode == RenderMode::Rasterization)
        {
            vk::DescriptorSetLayoutBinding cascade_matrices;
            cascade_matrices.binding = 3;
            cascade_matrices.descriptorCount = 1;
            cascade_matrices.descriptorType = vk::DescriptorType::eUniformBuffer;
            cascade_matrices.pImmutableSamplers = nullptr;
            cascade_matrices.stageFlags = vk::ShaderStageFlagBits::eVertex;

            std::vector<vk::DescriptorSetLayoutBinding> shadowmap_bindings = { camera_layout_binding, model_layout_binding, cascade_matrices, sample_layout_binding };

            layout_info.bindingCount = static_cast<uint32_t>(shadowmap_bindings.size());
            layout_info.pBindings = shadowmap_bindings.data();

            shadowmap_descriptor_set_layout = device->createDescriptorSetLayoutUnique(layout_info, nullptr, dispatch);
        }

        vk::DescriptorSetLayoutBinding pos_sample_layout_binding;
        pos_sample_layout_binding.binding = 0;
        pos_sample_layout_binding.descriptorCount = 1;
        pos_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        pos_sample_layout_binding.pImmutableSamplers = nullptr;
        pos_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding normal_sample_layout_binding;
        normal_sample_layout_binding.binding = 1;
        normal_sample_layout_binding.descriptorCount = 1;
        normal_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        normal_sample_layout_binding.pImmutableSamplers = nullptr;
        normal_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding albedo_sample_layout_binding;
        albedo_sample_layout_binding.binding = 2;
        albedo_sample_layout_binding.descriptorCount = 1;
        albedo_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        albedo_sample_layout_binding.pImmutableSamplers = nullptr;
        albedo_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding material_sample_layout_binding;
        material_sample_layout_binding.binding = 3;
        material_sample_layout_binding.descriptorCount = 1;
        material_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        material_sample_layout_binding.pImmutableSamplers = nullptr;
        material_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding camera_deferred_layout_binding;
        camera_deferred_layout_binding.binding = 4;
        camera_deferred_layout_binding.descriptorCount = 1;
        camera_deferred_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        camera_deferred_layout_binding.pImmutableSamplers = nullptr;
        camera_deferred_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding light_deferred_layout_binding;
        light_deferred_layout_binding.binding = 5;
        light_deferred_layout_binding.descriptorCount = 1;
        light_deferred_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        light_deferred_layout_binding.pImmutableSamplers = nullptr;
        light_deferred_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        std::vector<vk::DescriptorSetLayoutBinding> deferred_bindings = {
            pos_sample_layout_binding, 
            normal_sample_layout_binding, 
            albedo_sample_layout_binding,
            material_sample_layout_binding,
            camera_deferred_layout_binding,
            light_deferred_layout_binding
        };

        if (render_mode == RenderMode::Rasterization)
        {
            vk::DescriptorSetLayoutBinding lightsource_deferred_layout_binding;
            lightsource_deferred_layout_binding.binding = 6;
            lightsource_deferred_layout_binding.descriptorCount = 1;
            lightsource_deferred_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
            lightsource_deferred_layout_binding.pImmutableSamplers = nullptr;
            lightsource_deferred_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

            vk::DescriptorSetLayoutBinding shadowmap_deferred_layout_binding;
            shadowmap_deferred_layout_binding.binding = 7;
            shadowmap_deferred_layout_binding.descriptorCount = 1;
            shadowmap_deferred_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            shadowmap_deferred_layout_binding.pImmutableSamplers = nullptr;
            shadowmap_deferred_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
            deferred_bindings.push_back(lightsource_deferred_layout_binding);
            deferred_bindings.push_back(shadowmap_deferred_layout_binding);
        }

        layout_info.bindingCount = static_cast<uint32_t>(deferred_bindings.size());
        layout_info.pBindings = deferred_bindings.data();

        deferred_descriptor_set_layout = device->createDescriptorSetLayoutUnique(layout_info, nullptr, dispatch);

        if (RaytraceEnabled())
        {
            if (render_mode == RenderMode::Raytrace)
            {
                vk::DescriptorSetLayoutBinding raytrace_output_binding_albedo;
                raytrace_output_binding_albedo.binding = 0;
                raytrace_output_binding_albedo.descriptorCount = 1;
                raytrace_output_binding_albedo.descriptorType = vk::DescriptorType::eStorageImage;
                raytrace_output_binding_albedo.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

                vk::DescriptorSetLayoutBinding raytrace_output_binding_light;
                raytrace_output_binding_light.binding = 1;
                raytrace_output_binding_light.descriptorCount = 1;
                raytrace_output_binding_light.descriptorType = vk::DescriptorType::eStorageImage;
                raytrace_output_binding_light.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

                vk::DescriptorSetLayoutBinding camera_ubo_binding;
                camera_ubo_binding.binding = 2;
                camera_ubo_binding.descriptorCount = 1;
                camera_ubo_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
                camera_ubo_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

                vk::DescriptorSetLayoutBinding light_binding;
                light_binding.binding = 3;
                light_binding.descriptorCount = 1;
                light_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
                light_binding.pImmutableSamplers = nullptr;
                light_binding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR;

                vk::DescriptorSetLayoutBinding acceleration_structure_binding;
                acceleration_structure_binding.binding = 0;
                acceleration_structure_binding.descriptorCount = 1;
                acceleration_structure_binding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
                acceleration_structure_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR;

                vk::DescriptorSetLayoutBinding vertex_buffer_binding;
                vertex_buffer_binding.binding = 1;
                vertex_buffer_binding.descriptorCount = max_acceleration_binding_index;
                vertex_buffer_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
                vertex_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

                vk::DescriptorSetLayoutBinding index_buffer_binding;
                index_buffer_binding.binding = 2;
                index_buffer_binding.descriptorCount = max_acceleration_binding_index;
                index_buffer_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
                index_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

                vk::DescriptorSetLayoutBinding texture_bindings;
                texture_bindings.binding = 3;
                texture_bindings.descriptorCount = max_acceleration_binding_index;
                texture_bindings.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                texture_bindings.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

                vk::DescriptorSetLayoutBinding mesh_info_binding;
                mesh_info_binding.binding = 4;
                mesh_info_binding.descriptorCount = 1;
                mesh_info_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
                mesh_info_binding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

                std::vector<vk::DescriptorSetLayoutBinding> rtx_bindings_const
                {
                    acceleration_structure_binding,
                    vertex_buffer_binding,
                    index_buffer_binding,
                    texture_bindings,
                    mesh_info_binding
                };

                std::vector<vk::DescriptorSetLayoutBinding> rtx_bindings_dynamic
                {
                    raytrace_output_binding_albedo,
                    raytrace_output_binding_light,
                    camera_ubo_binding,
                    light_binding
                };

                vk::DescriptorSetLayoutCreateInfo rtx_layout_info_const;
                rtx_layout_info_const.bindingCount = static_cast<uint32_t>(rtx_bindings_const.size());
                rtx_layout_info_const.pBindings = rtx_bindings_const.data();

                std::vector<vk::DescriptorBindingFlags> binding_flags{ {}, vk::DescriptorBindingFlagBits::ePartiallyBound, vk::DescriptorBindingFlagBits::ePartiallyBound,
                    vk::DescriptorBindingFlagBits::ePartiallyBound, {} };
                vk::DescriptorSetLayoutBindingFlagsCreateInfo layout_flags{ static_cast<uint32_t>(binding_flags.size()), binding_flags.data() };
                rtx_layout_info_const.pNext = &layout_flags;

                vk::DescriptorSetLayoutCreateInfo rtx_layout_info_dynamic;
                rtx_layout_info_dynamic.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
                rtx_layout_info_dynamic.bindingCount = static_cast<uint32_t>(rtx_bindings_dynamic.size());
                rtx_layout_info_dynamic.pBindings = rtx_bindings_dynamic.data();

                rtx_descriptor_layout_const = device->createDescriptorSetLayoutUnique(rtx_layout_info_const, nullptr, dispatch);
                rtx_descriptor_layout_dynamic = device->createDescriptorSetLayoutUnique(rtx_layout_info_dynamic, nullptr, dispatch);
            }
            else
            {
                vk::DescriptorSetLayoutBinding raytrace_output_binding_light;
                raytrace_output_binding_light.binding = 0;
                raytrace_output_binding_light.descriptorCount = 1;
                raytrace_output_binding_light.descriptorType = vk::DescriptorType::eStorageImage;
                raytrace_output_binding_light.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

                vk::DescriptorSetLayoutBinding light_binding;
                light_binding.binding = 3;
                light_binding.descriptorCount = 1;
                light_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
                light_binding.pImmutableSamplers = nullptr;
                light_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR;

                vk::DescriptorSetLayoutBinding position_sample_layout_binding;
                position_sample_layout_binding.binding = 4;
                position_sample_layout_binding.descriptorCount = 1;
                position_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                position_sample_layout_binding.pImmutableSamplers = nullptr;
                position_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

                vk::DescriptorSetLayoutBinding normal_sample_layout_binding;
                normal_sample_layout_binding.binding = 5;
                normal_sample_layout_binding.descriptorCount = 1;
                normal_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                normal_sample_layout_binding.pImmutableSamplers = nullptr;
                normal_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

                vk::DescriptorSetLayoutBinding face_normal_sample_layout_binding;
                face_normal_sample_layout_binding.binding = 6;
                face_normal_sample_layout_binding.descriptorCount = 1;
                face_normal_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                face_normal_sample_layout_binding.pImmutableSamplers = nullptr;
                face_normal_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

                vk::DescriptorSetLayoutBinding material_index_sample_layout_binding;
                material_index_sample_layout_binding.binding = 7;
                material_index_sample_layout_binding.descriptorCount = 1;
                material_index_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                material_index_sample_layout_binding.pImmutableSamplers = nullptr;
                material_index_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

                vk::DescriptorSetLayoutBinding acceleration_structure_binding;
                acceleration_structure_binding.binding = 0;
                acceleration_structure_binding.descriptorCount = 1;
                acceleration_structure_binding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
                acceleration_structure_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR;

                vk::DescriptorSetLayoutBinding vertex_buffer_binding;
                vertex_buffer_binding.binding = 1;
                vertex_buffer_binding.descriptorCount = max_acceleration_binding_index;
                vertex_buffer_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
                vertex_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

                vk::DescriptorSetLayoutBinding index_buffer_binding;
                index_buffer_binding.binding = 2;
                index_buffer_binding.descriptorCount = max_acceleration_binding_index;
                index_buffer_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
                index_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

                vk::DescriptorSetLayoutBinding texture_bindings;
                texture_bindings.binding = 3;
                texture_bindings.descriptorCount = max_acceleration_binding_index;
                texture_bindings.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                texture_bindings.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

                vk::DescriptorSetLayoutBinding mesh_info_binding;
                mesh_info_binding.binding = 4;
                mesh_info_binding.descriptorCount = 1;
                mesh_info_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
                mesh_info_binding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

                std::vector<vk::DescriptorSetLayoutBinding> rtx_bindings_const
                {
                    acceleration_structure_binding,
                    vertex_buffer_binding,
                    index_buffer_binding,
                    texture_bindings,
                    mesh_info_binding
                };

                std::vector<vk::DescriptorSetLayoutBinding> rtx_bindings_dynamic
                {
                    raytrace_output_binding_light,
                    light_binding,
                    position_sample_layout_binding,
                    normal_sample_layout_binding,
                    face_normal_sample_layout_binding,
                    material_index_sample_layout_binding
                };

                vk::DescriptorSetLayoutCreateInfo rtx_layout_info_const;
                rtx_layout_info_const.bindingCount = static_cast<uint32_t>(rtx_bindings_const.size());
                rtx_layout_info_const.pBindings = rtx_bindings_const.data();

                std::vector<vk::DescriptorBindingFlags> binding_flags{ {}, vk::DescriptorBindingFlagBits::ePartiallyBound, vk::DescriptorBindingFlagBits::ePartiallyBound,
                    vk::DescriptorBindingFlagBits::ePartiallyBound, {} };
                vk::DescriptorSetLayoutBindingFlagsCreateInfo layout_flags{ static_cast<uint32_t>(binding_flags.size()), binding_flags.data() };
                rtx_layout_info_const.pNext = &layout_flags;

                vk::DescriptorSetLayoutCreateInfo rtx_layout_info_dynamic;
                rtx_layout_info_dynamic.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
                rtx_layout_info_dynamic.bindingCount = static_cast<uint32_t>(rtx_bindings_dynamic.size());
                rtx_layout_info_dynamic.pBindings = rtx_bindings_dynamic.data();

                rtx_descriptor_layout_const = device->createDescriptorSetLayoutUnique(rtx_layout_info_const, nullptr, dispatch);
                rtx_descriptor_layout_dynamic = device->createDescriptorSetLayoutUnique(rtx_layout_info_dynamic, nullptr, dispatch);
            }

            vk::DescriptorSetLayoutBinding position_sample_layout_binding;
            position_sample_layout_binding.binding = 0;
            position_sample_layout_binding.descriptorCount = 1;
            position_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            position_sample_layout_binding.pImmutableSamplers = nullptr;
            position_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

            vk::DescriptorSetLayoutBinding albedo_sample_layout_binding;
            albedo_sample_layout_binding.binding = 1;
            albedo_sample_layout_binding.descriptorCount = 1;
            albedo_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            albedo_sample_layout_binding.pImmutableSamplers = nullptr;
            albedo_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

            vk::DescriptorSetLayoutBinding light_sample_layout_binding;
            light_sample_layout_binding.binding = 2;
            light_sample_layout_binding.descriptorCount = 1;
            light_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            light_sample_layout_binding.pImmutableSamplers = nullptr;
            light_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

            vk::DescriptorSetLayoutBinding material_index_layout_binding;
            material_index_layout_binding.binding = 3;
            material_index_layout_binding.descriptorCount = 1;
            material_index_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            material_index_layout_binding.pImmutableSamplers = nullptr;
            material_index_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

            vk::DescriptorSetLayoutBinding particle_sample_layout_binding;
            particle_sample_layout_binding.binding = 4;
            particle_sample_layout_binding.descriptorCount = 1;
            particle_sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            particle_sample_layout_binding.pImmutableSamplers = nullptr;
            particle_sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

            vk::DescriptorSetLayoutBinding mesh_info_binding;
            mesh_info_binding.binding = 5;
            mesh_info_binding.descriptorCount = 1;
            mesh_info_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
            mesh_info_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

            vk::DescriptorSetLayoutBinding light_buffer_binding;
            light_buffer_binding.binding = 6;
            light_buffer_binding.descriptorCount = 1;
            light_buffer_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
            light_buffer_binding.pImmutableSamplers = nullptr;
            light_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

            vk::DescriptorSetLayoutBinding camera_buffer_binding;
            camera_buffer_binding.binding = 7;
            camera_buffer_binding.descriptorCount = 1;
            camera_buffer_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
            camera_buffer_binding.pImmutableSamplers = nullptr;
            camera_buffer_binding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

            std::vector<vk::DescriptorSetLayoutBinding> rtx_deferred_bindings = { position_sample_layout_binding, albedo_sample_layout_binding, light_sample_layout_binding,
                material_index_layout_binding, particle_sample_layout_binding, mesh_info_binding, light_buffer_binding, camera_buffer_binding };

            vk::DescriptorSetLayoutCreateInfo rtx_deferred_layout_info;
            rtx_deferred_layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
            rtx_deferred_layout_info.bindingCount = static_cast<uint32_t>(rtx_deferred_bindings.size());
            rtx_deferred_layout_info.pBindings = rtx_deferred_bindings.data();

            rtx_descriptor_layout_deferred = device->createDescriptorSetLayoutUnique(rtx_deferred_layout_info, nullptr);
        }
    }

    void Renderer::createGraphicsPipeline()
    {
        auto vertex_module = getShader("shaders/gbuffer_vert.spv");
        auto landscape_vertex_module = getShader("shaders/landscape_gbuffer_vert.spv");
        auto particle_vertex_module = getShader("shaders/particle_gbuffer_vert.spv");
        auto fragment_module = getShader("shaders/gbuffer_frag.spv");
        auto particle_fragment_module = getShader("shaders/particle_blend.spv");

        vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
        vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        vert_shader_stage_info.module = *vertex_module;
        vert_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo landscape_vert_shader_stage_info;
        landscape_vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        landscape_vert_shader_stage_info.module = *landscape_vertex_module;
        landscape_vert_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo particle_vert_shader_stage_info;
        particle_vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        particle_vert_shader_stage_info.module = *particle_vertex_module;
        particle_vert_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
        frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
        frag_shader_stage_info.module = *fragment_module;
        frag_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo particle_frag_shader_stage_info;
        particle_frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
        particle_frag_shader_stage_info.module = *particle_fragment_module;
        particle_frag_shader_stage_info.pName = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {vert_shader_stage_info, frag_shader_stage_info};
        std::array<vk::PipelineShaderStageCreateInfo, 2> landscape_shaderStages = {landscape_vert_shader_stage_info, frag_shader_stage_info};
        std::array<vk::PipelineShaderStageCreateInfo, 2> particle_shaderStages = {particle_vert_shader_stage_info, particle_frag_shader_stage_info};

        vk::PipelineVertexInputStateCreateInfo main_vertex_input_info;

        auto main_binding_descriptions = engine->settings.renderer_settings.model_vertex_input_binding_descriptions;
        auto main_attribute_descriptions = engine->settings.renderer_settings.model_vertex_input_attribute_descriptions;

        main_vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(main_binding_descriptions.size());
        main_vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(main_attribute_descriptions.size());
        main_vertex_input_info.pVertexBindingDescriptions = main_binding_descriptions.data();
        main_vertex_input_info.pVertexAttributeDescriptions = main_attribute_descriptions.data();

        vk::PipelineVertexInputStateCreateInfo landscape_vertex_input_info;

        auto landscape_binding_descriptions = engine->settings.renderer_settings.landscape_vertex_input_binding_descriptions;
        auto landscape_attribute_descriptions = engine->settings.renderer_settings.landscape_vertex_input_attribute_descriptions;

        landscape_vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(landscape_binding_descriptions.size());
        landscape_vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(landscape_attribute_descriptions.size());
        landscape_vertex_input_info.pVertexBindingDescriptions = landscape_binding_descriptions.data();
        landscape_vertex_input_info.pVertexAttributeDescriptions = landscape_attribute_descriptions.data();

        vk::PipelineVertexInputStateCreateInfo particle_vertex_input_info;

        auto particle_binding_descriptions = engine->settings.renderer_settings.particle_vertex_input_binding_descriptions;
        auto particle_attribute_descriptions = engine->settings.renderer_settings.particle_vertex_input_attribute_descriptions;

        particle_vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(particle_binding_descriptions.size());
        particle_vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(particle_attribute_descriptions.size());
        particle_vertex_input_info.pVertexBindingDescriptions = particle_binding_descriptions.data();
        particle_vertex_input_info.pVertexAttributeDescriptions = particle_attribute_descriptions.data();

        vk::PipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
        input_assembly.primitiveRestartEnable = false;

        vk::Viewport viewport;
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchain_extent.width;
        viewport.height = (float)swapchain_extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor;
        scissor.offset = vk::Offset2D{0, 0};
        scissor.extent = swapchain_extent;

        vk::PipelineViewportStateCreateInfo viewport_state;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        vk::PipelineRasterizationStateCreateInfo rasterizer;
        rasterizer.depthClampEnable = false;
        rasterizer.rasterizerDiscardEnable = false;
        rasterizer.polygonMode = vk::PolygonMode::eFill;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = vk::CullModeFlagBits::eNone;
        rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
        rasterizer.depthBiasEnable = false;

        vk::PipelineMultisampleStateCreateInfo multisampling;
        multisampling.sampleShadingEnable = false;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::PipelineDepthStencilStateCreateInfo depth_stencil;
        depth_stencil.depthTestEnable = true;
        depth_stencil.depthWriteEnable = true;
        depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
        depth_stencil.depthBoundsTestEnable = false;
        depth_stencil.stencilTestEnable = false;

        vk::PipelineColorBlendAttachmentState color_blend_attachment;
        color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        color_blend_attachment.blendEnable = false;
        color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;
        color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
        color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcColor;
        color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcColor;
        color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;

        std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states(5, color_blend_attachment);
        std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states_subpass1(1, color_blend_attachment);

        vk::PipelineColorBlendStateCreateInfo color_blending;
        color_blending.logicOpEnable = false;
        color_blending.logicOp = vk::LogicOp::eCopy;
        color_blending.attachmentCount = static_cast<uint32_t>(color_blend_attachment_states.size());
        color_blending.pAttachments = color_blend_attachment_states.data();
        color_blending.blendConstants[0] = 0.0f;
        color_blending.blendConstants[1] = 0.0f;
        color_blending.blendConstants[2] = 0.0f;
        color_blending.blendConstants[3] = 0.0f;

        vk::PipelineColorBlendStateCreateInfo color_blending_subpass1 = color_blending;
        color_blending_subpass1.attachmentCount = static_cast<uint32_t>(color_blend_attachment_states_subpass1.size());
        color_blending_subpass1.pAttachments = color_blend_attachment_states_subpass1.data();

        std::array<vk::DescriptorSetLayout, 1> descriptor_layouts = { *static_descriptor_set_layout };

        vk::PipelineLayoutCreateInfo pipeline_layout_info;
        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size());
        pipeline_layout_info.pSetLayouts = descriptor_layouts.data();

        //material index
        vk::PushConstantRange push_constant_range;
        push_constant_range.stageFlags = vk::ShaderStageFlagBits::eFragment;
        push_constant_range.size = 4;
        push_constant_range.offset = 0;

        pipeline_layout_info.pPushConstantRanges = &push_constant_range;
        pipeline_layout_info.pushConstantRangeCount = 1;

        pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_info, nullptr, dispatch);

        vk::GraphicsPipelineCreateInfo landscape_pipeline_info;
        landscape_pipeline_info.stageCount = static_cast<uint32_t>(landscape_shaderStages.size());
        landscape_pipeline_info.pStages = landscape_shaderStages.data();
        landscape_pipeline_info.pVertexInputState = &landscape_vertex_input_info;
        landscape_pipeline_info.pInputAssemblyState = &input_assembly;
        landscape_pipeline_info.pViewportState = &viewport_state;
        landscape_pipeline_info.pRasterizationState = &rasterizer;
        landscape_pipeline_info.pMultisampleState = &multisampling;
        landscape_pipeline_info.pDepthStencilState = &depth_stencil;
        landscape_pipeline_info.pColorBlendState = &color_blending;
        landscape_pipeline_info.layout = *pipeline_layout;
        landscape_pipeline_info.renderPass = *gbuffer_render_pass;
        landscape_pipeline_info.subpass = 0;
        landscape_pipeline_info.basePipelineHandle = nullptr;

        vk::GraphicsPipelineCreateInfo main_pipeline_info = landscape_pipeline_info;
        main_pipeline_info.stageCount = static_cast<uint32_t>(shaderStages.size());
        main_pipeline_info.pStages = shaderStages.data();
        main_pipeline_info.pVertexInputState = &main_vertex_input_info;

        vk::PipelineDepthStencilStateCreateInfo particle_depth_stencil;
        particle_depth_stencil.depthTestEnable = true;
        particle_depth_stencil.depthWriteEnable = false;
        particle_depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
        particle_depth_stencil.depthBoundsTestEnable = false;
        particle_depth_stencil.stencilTestEnable = false;

        vk::GraphicsPipelineCreateInfo particle_pipeline_info = landscape_pipeline_info;
        particle_pipeline_info.pDepthStencilState = &particle_depth_stencil;
        particle_pipeline_info.stageCount = static_cast<uint32_t>(particle_shaderStages.size());
        particle_pipeline_info.pStages = particle_shaderStages.data();
        particle_pipeline_info.pVertexInputState = &particle_vertex_input_info;
        particle_pipeline_info.pColorBlendState = &color_blending_subpass1;
        particle_pipeline_info.subpass = 1;

        main_pipeline_group.graphics_pipeline = device->createGraphicsPipelineUnique(nullptr, main_pipeline_info, nullptr, dispatch);
        landscape_pipeline_group.graphics_pipeline = device->createGraphicsPipelineUnique(nullptr, landscape_pipeline_info, nullptr, dispatch);
        particle_pipeline_group.graphics_pipeline = device->createGraphicsPipelineUnique(nullptr, particle_pipeline_info, nullptr, dispatch);

        fragment_module = getShader("shaders/blend.spv");

        frag_shader_stage_info.module = *fragment_module;

        shaderStages[1] = frag_shader_stage_info;
        landscape_shaderStages[1] = frag_shader_stage_info;

        main_pipeline_group.blended_graphics_pipeline = device->createGraphicsPipelineUnique(nullptr, main_pipeline_info, nullptr, dispatch);
        landscape_pipeline_group.blended_graphics_pipeline = device->createGraphicsPipelineUnique(nullptr, landscape_pipeline_info, nullptr, dispatch);
        particle_pipeline_group.blended_graphics_pipeline = device->createGraphicsPipelineUnique(nullptr, particle_pipeline_info, nullptr, dispatch);

        landscape_vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(1);
        landscape_vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(4);

        vertex_module = getShader("shaders/deferred.spv");
        vert_shader_stage_info.module = *vertex_module;

        if (render_mode == RenderMode::Rasterization)
        {
            fragment_module = getShader("shaders/deferred_raster.spv");
        }
        else
        {
            fragment_module = getShader("shaders/deferred_raytrace_hybrid.spv");
        }
        frag_shader_stage_info.module = *fragment_module;

        landscape_shaderStages[0] = vert_shader_stage_info;
        landscape_shaderStages[1] = frag_shader_stage_info;
        vk::GraphicsPipelineCreateInfo deferred_pipeline_info = landscape_pipeline_info;

        deferred_pipeline_info.renderPass = *render_pass;
        color_blend_attachment_states = std::vector<vk::PipelineColorBlendAttachmentState>(1, color_blend_attachment);
        color_blending.attachmentCount = static_cast<uint32_t>(color_blend_attachment_states.size());
        color_blending.pAttachments = color_blend_attachment_states.data();

        std::array<vk::DescriptorSetLayout, 1> deferred_descriptor_layouts = { *deferred_descriptor_set_layout };

        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(deferred_descriptor_layouts.size());
        pipeline_layout_info.pSetLayouts = deferred_descriptor_layouts.data();

        if (render_mode == RenderMode::Rasterization)
        {
            deferred_pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_info, nullptr, dispatch);

            deferred_pipeline_info.layout = *deferred_pipeline_layout;

            deferred_pipeline = device->createGraphicsPipelineUnique(nullptr, deferred_pipeline_info, nullptr, dispatch);

            landscape_vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(landscape_binding_descriptions.size());
            landscape_vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(landscape_attribute_descriptions.size());
            vk::PushConstantRange push_constant_range_shadowmap;
            push_constant_range_shadowmap.stageFlags = vk::ShaderStageFlagBits::eVertex;
            push_constant_range_shadowmap.size = sizeof(uint32_t);
            push_constant_range_shadowmap.offset = 4;

            color_blending.attachmentCount = 0;

            std::array<vk::DescriptorSetLayout, 1> shadowmap_descriptor_layouts = { *shadowmap_descriptor_set_layout };
            std::vector<vk::PushConstantRange> push_constant_ranges = { push_constant_range, push_constant_range_shadowmap };

            pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(shadowmap_descriptor_layouts.size());
            pipeline_layout_info.pSetLayouts = shadowmap_descriptor_layouts.data();
            pipeline_layout_info.pushConstantRangeCount = push_constant_ranges.size();
            pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();

            shadowmap_pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_info, nullptr, dispatch);

            main_pipeline_info.layout = *shadowmap_pipeline_layout;
            landscape_pipeline_info.layout = *shadowmap_pipeline_layout;
            particle_pipeline_info.layout = *shadowmap_pipeline_layout;

            main_pipeline_info.renderPass = *shadowmap_render_pass;
            landscape_pipeline_info.renderPass = *shadowmap_render_pass;
            particle_pipeline_info.renderPass = *shadowmap_render_pass;
            vertex_module = getShader("shaders/shadow_vert.spv");
            landscape_vertex_module = getShader("shaders/landscape_shadow_vert.spv");
            particle_vertex_module = getShader("shaders/particle_shadow_vert.spv");
            fragment_module = getShader("shaders/shadow_frag.spv");

            vert_shader_stage_info.module = *vertex_module;
            landscape_vert_shader_stage_info.module = *landscape_vertex_module;
            particle_vert_shader_stage_info.module = *particle_vertex_module;
            frag_shader_stage_info.module = *fragment_module;

            shaderStages = { vert_shader_stage_info, frag_shader_stage_info };
            landscape_shaderStages = { landscape_vert_shader_stage_info, frag_shader_stage_info };
            particle_shaderStages = { particle_vert_shader_stage_info, frag_shader_stage_info };

            main_pipeline_info.stageCount = static_cast<uint32_t>(shaderStages.size());
            main_pipeline_info.pStages = shaderStages.data();
            landscape_pipeline_info.stageCount = static_cast<uint32_t>(landscape_shaderStages.size());
            landscape_pipeline_info.pStages = landscape_shaderStages.data();
            particle_pipeline_info.stageCount = static_cast<uint32_t>(particle_shaderStages.size());
            particle_pipeline_info.pStages = particle_shaderStages.data();

            viewport.width = shadowmap_dimension;
            viewport.height = shadowmap_dimension;
            scissor.extent.width = shadowmap_dimension;
            scissor.extent.height = shadowmap_dimension;

            rasterizer.depthClampEnable = true;
            rasterizer.depthBiasEnable = true;

            std::vector<vk::DynamicState> dynamic_states = { vk::DynamicState::eDepthBias };
            vk::PipelineDynamicStateCreateInfo dynamic_state_ci;
            dynamic_state_ci.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
            dynamic_state_ci.pDynamicStates = dynamic_states.data();
            main_pipeline_info.pDynamicState = &dynamic_state_ci;
            landscape_pipeline_info.pDynamicState = &dynamic_state_ci;
            particle_pipeline_info.pDynamicState = &dynamic_state_ci;
            main_pipeline_group.blended_shadowmap_pipeline = device->createGraphicsPipelineUnique(nullptr, main_pipeline_info, nullptr, dispatch);
            landscape_pipeline_group.blended_shadowmap_pipeline = device->createGraphicsPipelineUnique(nullptr, landscape_pipeline_info, nullptr, dispatch);
            //particle_pipeline_group.blended_shadowmap_pipeline = device->createGraphicsPipelineUnique(nullptr, particle_pipeline_info, nullptr, dispatch);

            main_pipeline_info.stageCount = 1;
            main_pipeline_info.pStages = &vert_shader_stage_info;
            landscape_pipeline_info.stageCount = 1;
            landscape_pipeline_info.pStages = &landscape_vert_shader_stage_info;
            particle_pipeline_info.stageCount = 1;
            particle_pipeline_info.pStages = &particle_vert_shader_stage_info;
            main_pipeline_group.shadowmap_pipeline = device->createGraphicsPipelineUnique(nullptr, main_pipeline_info, nullptr, dispatch);
            landscape_pipeline_group.shadowmap_pipeline = device->createGraphicsPipelineUnique(nullptr, landscape_pipeline_info, nullptr, dispatch);
            //particle_pipeline_group.shadowmap_pipeline = device->createGraphicsPipelineUnique(nullptr, particle_pipeline_info, nullptr, dispatch);
        }
        if (RaytraceEnabled())
        {
            {
                //ray-tracing pipeline
                vk::UniqueShaderModule raygen_shader_module;
                if (render_mode == RenderMode::Raytrace)
                {
                    raygen_shader_module = getShader("shaders/raygen.spv");
                }
                else
                {
                    raygen_shader_module = getShader("shaders/raygen_hybrid.spv");
                }
                auto miss_shader_module = getShader("shaders/miss.spv");
                auto shadow_miss_shader_module = getShader("shaders/shadow_miss.spv");
                auto closest_hit_shader_module = getShader("shaders/closesthit.spv");
                auto color_hit_shader_module = getShader("shaders/color_hit.spv");
                auto landscape_closest_hit_shader_module = getShader("shaders/landscape_closest_hit.spv");
                auto landscape_color_hit_shader_module = getShader("shaders/landscape_color_hit.spv");
                auto particle_closest_hit_shader_module = getShader("shaders/particle_closest_hit.spv");
                auto particle_color_hit_shader_module = getShader("shaders/particle_color_hit.spv");
                auto particle_intersection_shader_module = getShader("shaders/particle_intersection.spv");

                vk::PipelineShaderStageCreateInfo raygen_stage_ci;
                raygen_stage_ci.stage = vk::ShaderStageFlagBits::eRaygenKHR;
                raygen_stage_ci.module = *raygen_shader_module;
                raygen_stage_ci.pName = "main";

                vk::PipelineShaderStageCreateInfo miss_stage_ci;
                miss_stage_ci.stage = vk::ShaderStageFlagBits::eMissKHR;
                miss_stage_ci.module = *miss_shader_module;
                miss_stage_ci.pName = "main";

                vk::PipelineShaderStageCreateInfo shadow_miss_stage_ci;
                shadow_miss_stage_ci.stage = vk::ShaderStageFlagBits::eMissKHR;
                shadow_miss_stage_ci.module = *shadow_miss_shader_module;
                shadow_miss_stage_ci.pName = "main";

                vk::PipelineShaderStageCreateInfo closest_stage_ci;
                closest_stage_ci.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
                closest_stage_ci.module = *closest_hit_shader_module;
                closest_stage_ci.pName = "main";

                vk::PipelineShaderStageCreateInfo color_hit_stage_ci;
                color_hit_stage_ci.stage = vk::ShaderStageFlagBits::eAnyHitKHR;
                color_hit_stage_ci.module = *color_hit_shader_module;
                color_hit_stage_ci.pName = "main";

                vk::PipelineShaderStageCreateInfo landscape_closest_stage_ci;
                landscape_closest_stage_ci.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
                landscape_closest_stage_ci.module = *landscape_closest_hit_shader_module;
                landscape_closest_stage_ci.pName = "main";

                vk::PipelineShaderStageCreateInfo landscape_color_hit_stage_ci;
                landscape_color_hit_stage_ci.stage = vk::ShaderStageFlagBits::eAnyHitKHR;
                landscape_color_hit_stage_ci.module = *landscape_color_hit_shader_module;
                landscape_color_hit_stage_ci.pName = "main";

                vk::PipelineShaderStageCreateInfo particle_closest_stage_ci;
                particle_closest_stage_ci.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
                particle_closest_stage_ci.module = *particle_closest_hit_shader_module;
                particle_closest_stage_ci.pName = "main";

                vk::PipelineShaderStageCreateInfo particle_color_hit_stage_ci;
                particle_color_hit_stage_ci.stage = vk::ShaderStageFlagBits::eAnyHitKHR;
                particle_color_hit_stage_ci.module = *particle_color_hit_shader_module;
                particle_color_hit_stage_ci.pName = "main";

                vk::PipelineShaderStageCreateInfo particle_intersection_stage_ci;
                particle_intersection_stage_ci.stage = vk::ShaderStageFlagBits::eIntersectionKHR;
                particle_intersection_stage_ci.module = *particle_intersection_shader_module;
                particle_intersection_stage_ci.pName = "main";

                std::vector<vk::PipelineShaderStageCreateInfo> shaders_ci = { raygen_stage_ci, miss_stage_ci, shadow_miss_stage_ci, closest_stage_ci, color_hit_stage_ci,
                    landscape_closest_stage_ci, landscape_color_hit_stage_ci, particle_closest_stage_ci, particle_color_hit_stage_ci, particle_intersection_stage_ci };

                std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_group_ci = {
                    {
                    vk::RayTracingShaderGroupTypeKHR::eGeneral,
                    0,
                    VK_SHADER_UNUSED_KHR,
                    VK_SHADER_UNUSED_KHR,
                    VK_SHADER_UNUSED_KHR
                    },
                    {
                    vk::RayTracingShaderGroupTypeKHR::eGeneral,
                    1,
                    VK_SHADER_UNUSED_KHR,
                    VK_SHADER_UNUSED_KHR,
                    VK_SHADER_UNUSED_KHR
                    },
                    {
                    vk::RayTracingShaderGroupTypeKHR::eGeneral,
                    2,
                    VK_SHADER_UNUSED_KHR,
                    VK_SHADER_UNUSED_KHR,
                    VK_SHADER_UNUSED_KHR
                    }
                };
                for (int i = 0; i < shaders_per_group; ++i)
                {
                    shader_group_ci.emplace_back(
                        vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                        VK_SHADER_UNUSED_KHR,
                        3,
                        4,
                        VK_SHADER_UNUSED_KHR
                    );
                }
                for (int i = 0; i < shaders_per_group; ++i)
                {
                    shader_group_ci.emplace_back(
                        vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                        VK_SHADER_UNUSED_KHR,
                        VK_SHADER_UNUSED_KHR,
                        4,
                        VK_SHADER_UNUSED_KHR
                    );
                }
                for (int i = 0; i < shaders_per_group; ++i)
                {
                    shader_group_ci.emplace_back(
                        vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                        VK_SHADER_UNUSED_KHR,
                        5,
                        6,
                        VK_SHADER_UNUSED_KHR
                    );
                }
                for (int i = 0; i < shaders_per_group; ++i)
                {
                    shader_group_ci.emplace_back(
                        vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                        VK_SHADER_UNUSED_KHR,
                        VK_SHADER_UNUSED_KHR,
                        6,
                        VK_SHADER_UNUSED_KHR
                    );
                }

                for (int i = 0; i < shaders_per_group; ++i)
                {
                    shader_group_ci.emplace_back(
                        vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                        VK_SHADER_UNUSED_KHR,
                        7,
                        8,
                        VK_SHADER_UNUSED_KHR
                    );
                }

                for (int i = 0; i < shaders_per_group; ++i)
                {
                    shader_group_ci.emplace_back(
                        vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                        VK_SHADER_UNUSED_KHR,
                        VK_SHADER_UNUSED_KHR,
                        8,
                        VK_SHADER_UNUSED_KHR
                    );
                }

                std::vector<vk::DescriptorSetLayout> rtx_descriptor_layouts = { *rtx_descriptor_layout_const, *rtx_descriptor_layout_dynamic };
                vk::PipelineLayoutCreateInfo rtx_pipeline_layout_ci;
                rtx_pipeline_layout_ci.pSetLayouts = rtx_descriptor_layouts.data();
                rtx_pipeline_layout_ci.setLayoutCount = static_cast<uint32_t>(rtx_descriptor_layouts.size());

                rtx_pipeline_layout = device->createPipelineLayoutUnique(rtx_pipeline_layout_ci, nullptr, dispatch);

                vk::RayTracingPipelineCreateInfoKHR rtx_pipeline_ci;
                rtx_pipeline_ci.maxRecursionDepth = 3;
                rtx_pipeline_ci.stageCount = static_cast<uint32_t>(shaders_ci.size());
                rtx_pipeline_ci.pStages = shaders_ci.data();
                rtx_pipeline_ci.groupCount = static_cast<uint32_t>(shader_group_ci.size());
                rtx_pipeline_ci.pGroups = shader_group_ci.data();
                rtx_pipeline_ci.layout = *rtx_pipeline_layout;

                auto result = device->createRayTracingPipelineKHRUnique(nullptr, rtx_pipeline_ci, nullptr, dispatch);
                rtx_pipeline = std::move(result.value);
            }
            {
                //deferred pipeline
                auto vertex_module = getShader("shaders/quad.spv");
                vk::UniqueShaderModule fragment_module;
                if (render_mode == RenderMode::Raytrace)
                {
                    fragment_module = getShader("shaders/deferred_raytrace.spv");
                }
                else
                {
                    fragment_module = getShader("shaders/deferred_raytrace_hybrid.spv");
                }

                vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
                vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
                vert_shader_stage_info.module = *vertex_module;
                vert_shader_stage_info.pName = "main";

                vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
                frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
                frag_shader_stage_info.module = *fragment_module;
                frag_shader_stage_info.pName = "main";

                std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {vert_shader_stage_info, frag_shader_stage_info};

                vk::PipelineVertexInputStateCreateInfo vertex_input_info;

                vertex_input_info.vertexBindingDescriptionCount = 0;
                vertex_input_info.vertexAttributeDescriptionCount = 0;

                vk::PipelineInputAssemblyStateCreateInfo input_assembly = {};
                input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
                input_assembly.primitiveRestartEnable = false;

                vk::Viewport viewport;
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = (float)swapchain_extent.width;
                viewport.height = (float)swapchain_extent.height;
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;

                vk::Rect2D scissor;
                scissor.offset = vk::Offset2D{0, 0};
                scissor.extent = swapchain_extent;

                vk::PipelineViewportStateCreateInfo viewport_state;
                viewport_state.viewportCount = 1;
                viewport_state.pViewports = &viewport;
                viewport_state.scissorCount = 1;
                viewport_state.pScissors = &scissor;

                vk::PipelineRasterizationStateCreateInfo rasterizer;
                rasterizer.depthClampEnable = false;
                rasterizer.rasterizerDiscardEnable = false;
                rasterizer.polygonMode = vk::PolygonMode::eFill;
                rasterizer.lineWidth = 1.0f;
                rasterizer.cullMode = vk::CullModeFlagBits::eNone;
                rasterizer.frontFace = vk::FrontFace::eClockwise;
                rasterizer.depthBiasEnable = false;

                vk::PipelineMultisampleStateCreateInfo multisampling;
                multisampling.sampleShadingEnable = false;
                multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

                vk::PipelineDepthStencilStateCreateInfo depth_stencil;
                depth_stencil.depthTestEnable = false;
                depth_stencil.depthWriteEnable = false;
                depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
                depth_stencil.depthBoundsTestEnable = false;
                depth_stencil.stencilTestEnable = false;

                vk::PipelineColorBlendAttachmentState color_blend_attachment;
                color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
                color_blend_attachment.blendEnable = false;

                std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states(1, color_blend_attachment);

                vk::PipelineColorBlendStateCreateInfo color_blending;
                color_blending.logicOpEnable = false;
                color_blending.logicOp = vk::LogicOp::eCopy;
                color_blending.attachmentCount = static_cast<uint32_t>(color_blend_attachment_states.size());
                color_blending.pAttachments = color_blend_attachment_states.data();
                color_blending.blendConstants[0] = 0.0f;
                color_blending.blendConstants[1] = 0.0f;
                color_blending.blendConstants[2] = 0.0f;
                color_blending.blendConstants[3] = 0.0f;

                std::array<vk::DescriptorSetLayout, 1> descriptor_layouts = { *rtx_descriptor_layout_deferred };

                vk::PipelineLayoutCreateInfo pipeline_layout_info;
                pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size());
                pipeline_layout_info.pSetLayouts = descriptor_layouts.data();

                rtx_deferred_pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_info, nullptr, dispatch);

                vk::GraphicsPipelineCreateInfo pipeline_info;
                pipeline_info.stageCount = static_cast<uint32_t>(shaderStages.size());
                pipeline_info.pStages = shaderStages.data();
                pipeline_info.pVertexInputState = &vertex_input_info;
                pipeline_info.pInputAssemblyState = &input_assembly;
                pipeline_info.pViewportState = &viewport_state;
                pipeline_info.pRasterizationState = &rasterizer;
                pipeline_info.pMultisampleState = &multisampling;
                pipeline_info.pDepthStencilState = &depth_stencil;
                pipeline_info.pColorBlendState = &color_blending;
                pipeline_info.layout = *rtx_deferred_pipeline_layout;
                pipeline_info.renderPass = *rtx_render_pass;
                pipeline_info.subpass = 0;
                pipeline_info.basePipelineHandle = nullptr;

                rtx_deferred_pipeline = device->createGraphicsPipelineUnique(nullptr, pipeline_info, nullptr, dispatch);
            }
        }
    }

    void Renderer::createDepthImage()
    {
        auto format = getDepthFormat();

        depth_image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = format;
        image_view_info.image = depth_image->image;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        depth_image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);
    }

    void Renderer::createFramebuffers()
    {
        frame_buffers.clear();
        for (auto& swapchain_image_view : swapchain_image_views) {
            std::array<vk::ImageView, 2> attachments = {
                *swapchain_image_view,
                *depth_image_view
            };

            vk::FramebufferCreateInfo framebuffer_info = {};
            framebuffer_info.renderPass = *render_pass;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = swapchain_extent.width;
            framebuffer_info.height = swapchain_extent.height;
            framebuffer_info.layers = 1;

            frame_buffers.push_back(device->createFramebufferUnique(framebuffer_info, nullptr, dispatch));
        }
    }

    void Renderer::createSyncs()
    {
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
        for (uint32_t i = 0; i < max_pending_frames; ++i)
        {
            frame_fences.push_back(device->createFenceUnique(fenceInfo, nullptr, dispatch));
            image_ready_sem.push_back(device->createSemaphoreUnique({}, nullptr, dispatch));
            frame_finish_sem.push_back(device->createSemaphoreUnique({}, nullptr, dispatch));
        }
        gbuffer_sem = device->createSemaphoreUnique({}, nullptr, dispatch);
        compute_sem = device->createSemaphoreUnique({}, nullptr, dispatch);
    }

    void Renderer::createCommandPool()
    {
        auto graphics_queue = std::get<0>(getQueueFamilies(physical_device));

        vk::CommandPoolCreateInfo pool_info = {};
        pool_info.queueFamilyIndex = graphics_queue.value();

        command_pool = device->createCommandPoolUnique(pool_info, nullptr, dispatch);
    }

    void Renderer::createShadowmapResources()
    {
        if (render_mode == RenderMode::Rasterization)
        {

            auto format = getDepthFormat();

            shadowmap_image = memory_manager->GetImage(shadowmap_dimension, shadowmap_dimension, format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, shadowmap_cascades);

            vk::ImageViewCreateInfo image_view_info;
            image_view_info.image = shadowmap_image->image;
            image_view_info.viewType = vk::ImageViewType::e2DArray;
            image_view_info.format = format;
            image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
            image_view_info.subresourceRange.baseMipLevel = 0;
            image_view_info.subresourceRange.levelCount = 1;
            image_view_info.subresourceRange.baseArrayLayer = 0;
            image_view_info.subresourceRange.layerCount = shadowmap_cascades;
            shadowmap_image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);

            for (uint32_t i = 0; i < shadowmap_cascades; ++i)
            {
                ShadowmapCascade& cascade = cascades[i];
                image_view_info.subresourceRange.baseArrayLayer = i;
                image_view_info.subresourceRange.layerCount = 1;

                cascade.shadowmap_image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);

                std::array<vk::ImageView, 1> attachments = {
                    *cascade.shadowmap_image_view
                };

                vk::FramebufferCreateInfo framebuffer_info = {};
                framebuffer_info.renderPass = *shadowmap_render_pass;
                framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
                framebuffer_info.pAttachments = attachments.data();
                framebuffer_info.width = shadowmap_dimension;
                framebuffer_info.height = shadowmap_dimension;
                framebuffer_info.layers = 1;

                cascade.shadowmap_frame_buffer = device->createFramebufferUnique(framebuffer_info, nullptr, dispatch);
            }

            vk::SamplerCreateInfo sampler_info = {};
            sampler_info.magFilter = vk::Filter::eLinear;
            sampler_info.minFilter = vk::Filter::eLinear;
            sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
            sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
            sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
            sampler_info.anisotropyEnable = false;
            sampler_info.maxAnisotropy = 1.f;
            sampler_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
            sampler_info.unnormalizedCoordinates = false;
            sampler_info.compareEnable = false;
            sampler_info.compareOp = vk::CompareOp::eAlways;
            sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;

            shadowmap_sampler = device->createSamplerUnique(sampler_info, nullptr, dispatch);
        }
    }

    void Renderer::createGBufferResources()
    {
        gbuffer.position.image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.normal.image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.face_normal.image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.albedo.image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.particle.image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.material.image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, vk::Format::eR16Uint, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
        gbuffer.depth.image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, getDepthFormat(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = gbuffer.position.image->image;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = vk::Format::eR32G32B32A32Sfloat;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;
        gbuffer.position.image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);
        image_view_info.image = gbuffer.normal.image->image;
        gbuffer.normal.image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);
        image_view_info.image = gbuffer.face_normal.image->image;
        gbuffer.face_normal.image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);
        image_view_info.image = gbuffer.albedo.image->image;
        image_view_info.format = vk::Format::eR8G8B8A8Unorm;
        gbuffer.albedo.image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);
        image_view_info.image = gbuffer.particle.image->image;
        gbuffer.particle.image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);
        image_view_info.image = gbuffer.material.image->image;
        image_view_info.format = vk::Format::eR16Uint;
        gbuffer.material.image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);
        image_view_info.image = gbuffer.depth.image->image;
        image_view_info.format = getDepthFormat();
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        gbuffer.depth.image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);

        std::vector<vk::ImageView> attachments = { *gbuffer.position.image_view, *gbuffer.normal.image_view, *gbuffer.face_normal.image_view,
            *gbuffer.albedo.image_view, *gbuffer.particle.image_view, *gbuffer.material.image_view, *gbuffer.depth.image_view };

        vk::FramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.renderPass = *gbuffer_render_pass;
        framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = swapchain_extent.width;
        framebuffer_info.height = swapchain_extent.height;
        framebuffer_info.layers = 1;

        gbuffer.frame_buffer = device->createFramebufferUnique(framebuffer_info, nullptr, dispatch);

        vk::SamplerCreateInfo sampler_info = {};
        sampler_info.magFilter = vk::Filter::eNearest;
        sampler_info.minFilter = vk::Filter::eNearest;
        sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        sampler_info.anisotropyEnable = false;
        sampler_info.maxAnisotropy = 1.f;
        sampler_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
        sampler_info.unnormalizedCoordinates = false;
        sampler_info.compareEnable = false;
        sampler_info.compareOp = vk::CompareOp::eAlways;
        sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;

        gbuffer.sampler = device->createSamplerUnique(sampler_info, nullptr, dispatch);

        if (RaytraceEnabled())
        {
            rtx_gbuffer.albedo.image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);
            rtx_gbuffer.light.image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, vk::MemoryPropertyFlagBits::eDeviceLocal);

            vk::ImageViewCreateInfo image_view_info;
            image_view_info.image = rtx_gbuffer.albedo.image->image;
            image_view_info.viewType = vk::ImageViewType::e2D;
            image_view_info.format = vk::Format::eR8G8B8A8Unorm;
            image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            image_view_info.subresourceRange.baseMipLevel = 0;
            image_view_info.subresourceRange.levelCount = 1;
            image_view_info.subresourceRange.baseArrayLayer = 0;
            image_view_info.subresourceRange.layerCount = 1;
            rtx_gbuffer.albedo.image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);
            image_view_info.image = rtx_gbuffer.light.image->image;
            image_view_info.format = vk::Format::eR32G32B32A32Sfloat;
            rtx_gbuffer.light.image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);

            vk::SamplerCreateInfo sampler_info = {};
            sampler_info.magFilter = vk::Filter::eNearest;
            sampler_info.minFilter = vk::Filter::eNearest;
            sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
            sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
            sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
            sampler_info.anisotropyEnable = false;
            sampler_info.maxAnisotropy = 1.f;
            sampler_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
            sampler_info.unnormalizedCoordinates = false;
            sampler_info.compareEnable = false;
            sampler_info.compareOp = vk::CompareOp::eAlways;
            sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;

            rtx_gbuffer.sampler = device->createSamplerUnique(sampler_info, nullptr, dispatch);
        }

        mesh_info_buffer = memory_manager->GetBuffer(max_acceleration_binding_index * sizeof(MeshInfo) * getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible);
        mesh_info_buffer_mapped = (MeshInfo*)mesh_info_buffer->map(0, max_acceleration_binding_index * sizeof(MeshInfo) * getImageCount(), {});
    }

    void Renderer::createDeferredCommandBuffer()
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = getImageCount();

        deferred_command_buffers = device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, dispatch);

        if (render_mode == RenderMode::Rasterization)
        {
            for (int i = 0; i < deferred_command_buffers.size(); ++i)
            {
                vk::CommandBuffer buffer = *deferred_command_buffers[i];
                vk::CommandBufferBeginInfo begin_info = {};
                begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

                buffer.begin(begin_info, dispatch);

                std::array<vk::ClearValue, 2> clearValues;
                clearValues[0].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
                clearValues[1].depthStencil = vk::ClearDepthStencilValue{ 1.f, 0 };

                vk::RenderPassBeginInfo renderpass_info;
                renderpass_info.renderPass = *render_pass;
                renderpass_info.clearValueCount = static_cast<uint32_t>(clearValues.size());
                renderpass_info.pClearValues = clearValues.data();
                renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };
                renderpass_info.renderArea.extent = swapchain_extent;
                renderpass_info.framebuffer = *frame_buffers[i];
                buffer.beginRenderPass(renderpass_info, vk::SubpassContents::eInline, dispatch);

                vk::DescriptorImageInfo pos_info;
                pos_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                pos_info.imageView = *gbuffer.position.image_view;
                pos_info.sampler = *gbuffer.sampler;

                vk::DescriptorImageInfo normal_info;
                normal_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                normal_info.imageView = *gbuffer.normal.image_view;
                normal_info.sampler = *gbuffer.sampler;

                vk::DescriptorImageInfo albedo_info;
                albedo_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                albedo_info.imageView = *gbuffer.albedo.image_view;
                albedo_info.sampler = *gbuffer.sampler;

                vk::DescriptorImageInfo material_info;
                material_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                material_info.imageView = *gbuffer.material.image_view;
                material_info.sampler = *gbuffer.sampler;

                vk::DescriptorBufferInfo camera_buffer_info;
                camera_buffer_info.buffer = engine->camera->view_proj_ubo->buffer;
                camera_buffer_info.offset = i * uniform_buffer_align_up(sizeof(Camera::CameraData));
                camera_buffer_info.range = sizeof(Camera::CameraData);

                std::vector<vk::WriteDescriptorSet> descriptorWrites{ 5 };

                descriptorWrites[0].dstSet = nullptr;
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pImageInfo = &pos_info;

                descriptorWrites[1].dstSet = nullptr;
                descriptorWrites[1].dstBinding = 1;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &normal_info;

                descriptorWrites[2].dstSet = nullptr;
                descriptorWrites[2].dstBinding = 2;
                descriptorWrites[2].dstArrayElement = 0;
                descriptorWrites[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[2].descriptorCount = 1;
                descriptorWrites[2].pImageInfo = &albedo_info;

                descriptorWrites[3].dstSet = nullptr;
                descriptorWrites[3].dstBinding = 3;
                descriptorWrites[3].dstArrayElement = 0;
                descriptorWrites[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[3].descriptorCount = 1;
                descriptorWrites[3].pImageInfo = &material_info;

                descriptorWrites[4].dstSet = nullptr;
                descriptorWrites[4].dstBinding = 4;
                descriptorWrites[4].dstArrayElement = 0;
                descriptorWrites[4].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[4].descriptorCount = 1;
                descriptorWrites[4].pBufferInfo = &camera_buffer_info;

                vk::DescriptorBufferInfo light_buffer_info;
                light_buffer_info.buffer = engine->lights.light_buffer->buffer;
                light_buffer_info.offset = i * uniform_buffer_align_up(sizeof(engine->lights.light));
                light_buffer_info.range = sizeof(engine->lights.light);

                vk::DescriptorImageInfo shadowmap_image_info;
                shadowmap_image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                shadowmap_image_info.imageView = *shadowmap_image_view;
                shadowmap_image_info.sampler = *shadowmap_sampler;

                vk::DescriptorBufferInfo cascade_buffer_info;
                cascade_buffer_info.buffer = engine->camera->cascade_data_ubo->buffer;
                cascade_buffer_info.offset = i * uniform_buffer_align_up(sizeof(engine->camera->cascade_data));
                cascade_buffer_info.range = sizeof(engine->camera->cascade_data);

                descriptorWrites.resize(8);

                descriptorWrites[5].dstSet = nullptr;
                descriptorWrites[5].dstBinding = 5;
                descriptorWrites[5].dstArrayElement = 0;
                descriptorWrites[5].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[5].descriptorCount = 1;
                descriptorWrites[5].pBufferInfo = &light_buffer_info;

                descriptorWrites[6].dstSet = nullptr;
                descriptorWrites[6].dstBinding = 6;
                descriptorWrites[6].dstArrayElement = 0;
                descriptorWrites[6].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[6].descriptorCount = 1;
                descriptorWrites[6].pBufferInfo = &cascade_buffer_info;

                descriptorWrites[7].dstSet = nullptr;
                descriptorWrites[7].dstBinding = 7;
                descriptorWrites[7].dstArrayElement = 0;
                descriptorWrites[7].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[7].descriptorCount = 1;
                descriptorWrites[7].pImageInfo = &shadowmap_image_info;

                buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *deferred_pipeline_layout, 0, descriptorWrites, dispatch);

                buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *deferred_pipeline);

                vk::DeviceSize offsets = 0;
                buffer.bindVertexBuffers(0, quad.vertex_buffer->buffer, offsets, dispatch);
                buffer.bindIndexBuffer(quad.index_buffer->buffer, offsets, vk::IndexType::eUint32, dispatch);
                buffer.drawIndexed(6, 1, 0, 0, 0, dispatch);

                buffer.endRenderPass(dispatch);
                buffer.end(dispatch);
            }
        }
        else if (render_mode == RenderMode::Raytrace)
        {
            for (int i = 0; i < deferred_command_buffers.size(); ++i)
            {
                vk::CommandBuffer buffer = *deferred_command_buffers[i];

                vk::CommandBufferBeginInfo begin_info = {};
                begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

                buffer.begin(begin_info);

                std::array<vk::ClearValue, 2> clear_values;
                clear_values[0].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
                clear_values[1].depthStencil = 1.f;

                vk::RenderPassBeginInfo renderpass_info;
                renderpass_info.renderPass = *rtx_render_pass;
                renderpass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
                renderpass_info.pClearValues = clear_values.data();
                renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };
                renderpass_info.renderArea.extent = swapchain_extent;
                renderpass_info.framebuffer = *frame_buffers[i];
                buffer.beginRenderPass(renderpass_info, vk::SubpassContents::eInline, dispatch);

                buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *rtx_deferred_pipeline);

                vk::DescriptorImageInfo albedo_info;
                albedo_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                albedo_info.imageView = *rtx_gbuffer.albedo.image_view;
                albedo_info.sampler = *rtx_gbuffer.sampler;

                vk::DescriptorImageInfo light_info;
                light_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                light_info.imageView = *rtx_gbuffer.light.image_view;
                light_info.sampler = *rtx_gbuffer.sampler;

                vk::DescriptorImageInfo material_index_info;
                material_index_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                material_index_info.imageView = *gbuffer.material.image_view;
                material_index_info.sampler = *rtx_gbuffer.sampler;

                vk::DescriptorBufferInfo mesh_info;
                mesh_info.buffer = mesh_info_buffer->buffer;
                mesh_info.offset = sizeof(Renderer::MeshInfo) * max_acceleration_binding_index * i;
                mesh_info.range = sizeof(Renderer::MeshInfo) * max_acceleration_binding_index;

                std::vector<vk::WriteDescriptorSet> descriptorWrites {4};

                descriptorWrites[0].dstSet = nullptr;
                descriptorWrites[0].dstBinding = 1;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pImageInfo = &albedo_info;

                descriptorWrites[1].dstSet = nullptr;
                descriptorWrites[1].dstBinding = 2;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &light_info;

                descriptorWrites[2].dstSet = nullptr;
                descriptorWrites[2].dstBinding = 3;
                descriptorWrites[2].dstArrayElement = 0;
                descriptorWrites[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[2].descriptorCount = 1;
                descriptorWrites[2].pImageInfo = &material_index_info;

                descriptorWrites[3].dstSet = nullptr;
                descriptorWrites[3].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[3].dstBinding = 5;
                descriptorWrites[3].dstArrayElement = 0;
                descriptorWrites[3].descriptorCount = 1;
                descriptorWrites[3].pBufferInfo = &mesh_info;

                buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *rtx_deferred_pipeline_layout, 0, descriptorWrites);

                buffer.draw(3, 1, 0, 0);

                buffer.endRenderPass();
                buffer.end();
            }
        }
        else if (render_mode == RenderMode::Hybrid)
        {
            for (int i = 0; i < deferred_command_buffers.size(); ++i)
            {
                vk::CommandBuffer buffer = *deferred_command_buffers[i];

                vk::CommandBufferBeginInfo begin_info = {};
                begin_info.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

                buffer.begin(begin_info);

                std::array<vk::ClearValue, 2> clear_values;
                clear_values[0].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
                clear_values[1].depthStencil = 1.f;

                vk::RenderPassBeginInfo renderpass_info;
                renderpass_info.renderPass = *rtx_render_pass;
                renderpass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
                renderpass_info.pClearValues = clear_values.data();
                renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };
                renderpass_info.renderArea.extent = swapchain_extent;
                renderpass_info.framebuffer = *frame_buffers[i];
                buffer.beginRenderPass(renderpass_info, vk::SubpassContents::eInline, dispatch);

                buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *rtx_deferred_pipeline);

                vk::DescriptorImageInfo albedo_info;
                albedo_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                albedo_info.imageView = *gbuffer.albedo.image_view;
                albedo_info.sampler = *gbuffer.sampler;

                vk::DescriptorImageInfo light_info;
                light_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                light_info.imageView = *rtx_gbuffer.light.image_view;
                light_info.sampler = *rtx_gbuffer.sampler;

                vk::DescriptorImageInfo position_info;
                position_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                position_info.imageView = *gbuffer.position.image_view;
                position_info.sampler = *gbuffer.sampler;

                vk::DescriptorImageInfo deferred_material_index_info;
                deferred_material_index_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                deferred_material_index_info.imageView = *gbuffer.material.image_view;
                deferred_material_index_info.sampler = *gbuffer.sampler;

                vk::DescriptorImageInfo particle_info;
                particle_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                particle_info.imageView = *gbuffer.particle.image_view;
                particle_info.sampler = *gbuffer.sampler;

                vk::DescriptorBufferInfo mesh_info;
                mesh_info.buffer = mesh_info_buffer->buffer;
                mesh_info.offset = sizeof(Renderer::MeshInfo) * max_acceleration_binding_index * i;
                mesh_info.range = sizeof(Renderer::MeshInfo) * max_acceleration_binding_index;

                vk::DescriptorBufferInfo light_buffer_info;
                light_buffer_info.buffer = engine->lights.light_buffer->buffer;
                light_buffer_info.offset = i * uniform_buffer_align_up(sizeof(engine->lights.light));
                light_buffer_info.range = sizeof(engine->lights.light);

                vk::DescriptorBufferInfo camera_buffer_info;
                camera_buffer_info.buffer = engine->camera->view_proj_ubo->buffer;
                camera_buffer_info.offset = i * uniform_buffer_align_up(sizeof(Camera::CameraData));
                camera_buffer_info.range = sizeof(Camera::CameraData);

                std::vector<vk::WriteDescriptorSet> descriptorWrites {8};

                descriptorWrites[0].dstSet = nullptr;
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pImageInfo = &position_info;

                descriptorWrites[1].dstSet = nullptr;
                descriptorWrites[1].dstBinding = 1;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &albedo_info;

                descriptorWrites[2].dstSet = nullptr;
                descriptorWrites[2].dstBinding = 2;
                descriptorWrites[2].dstArrayElement = 0;
                descriptorWrites[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[2].descriptorCount = 1;
                descriptorWrites[2].pImageInfo = &light_info;

                descriptorWrites[3].dstSet = nullptr;
                descriptorWrites[3].dstBinding = 3;
                descriptorWrites[3].dstArrayElement = 0;
                descriptorWrites[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[3].descriptorCount = 1;
                descriptorWrites[3].pImageInfo = &deferred_material_index_info;

                descriptorWrites[4].dstSet = nullptr;
                descriptorWrites[4].dstBinding = 4;
                descriptorWrites[4].dstArrayElement = 0;
                descriptorWrites[4].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptorWrites[4].descriptorCount = 1;
                descriptorWrites[4].pImageInfo = &particle_info;

                descriptorWrites[5].dstSet = nullptr;
                descriptorWrites[5].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[5].dstBinding = 5;
                descriptorWrites[5].dstArrayElement = 0;
                descriptorWrites[5].descriptorCount = 1;
                descriptorWrites[5].pBufferInfo = &mesh_info;

                descriptorWrites[6].dstSet = nullptr;
                descriptorWrites[6].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[6].dstBinding = 6;
                descriptorWrites[6].dstArrayElement = 0;
                descriptorWrites[6].descriptorCount = 1;
                descriptorWrites[6].pBufferInfo = &light_buffer_info;

                descriptorWrites[7].dstSet = nullptr;
                descriptorWrites[7].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptorWrites[7].dstBinding = 7;
                descriptorWrites[7].dstArrayElement = 0;
                descriptorWrites[7].descriptorCount = 1;
                descriptorWrites[7].pBufferInfo = &camera_buffer_info;

                buffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, *rtx_deferred_pipeline_layout, 0, descriptorWrites, dispatch);

                buffer.draw(3, 1, 0, 0);

                buffer.endRenderPass(dispatch);

                buffer.end();
            }
        }
    }

    void Renderer::createQuad()
    {
        struct Vertex
        {
            float pos[3];
            float normal[3];
            float color[3];
            float uv[2];
            float _pad32;
        };

        std::vector<Vertex> vertex_buffer {
            {{1.f, 1.f, 1.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}, 0.f},
            {{-1.f, 1.f, 1.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}, 0.f},
            {{-1.f, -1.f, 1.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}, 0.f},
            {{1.f, -1.f, 1.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}, 0.f}
        };

        quad.vertex_buffer = memory_manager->GetBuffer(vertex_buffer.size() * sizeof(Vertex), vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        void* buf_mem = quad.vertex_buffer->map(0, vertex_buffer.size() * sizeof(Vertex), {});
        memcpy(buf_mem, vertex_buffer.data(), vertex_buffer.size() * sizeof(Vertex));
        quad.vertex_buffer->unmap();

        std::vector<uint32_t> index_buffer = { 0,1,2,2,3,0 };
        for (uint32_t i = 0; i < 3; ++i)
        {
            uint32_t indices[6] = { 0,1,2, 2,3,0 };
            for (auto index : indices)
            {
                index_buffer.push_back(i * 4 + index);
            }
        }
        quad.index_count = static_cast<uint32_t>(index_buffer.size());

        quad.index_buffer = memory_manager->GetBuffer(index_buffer.size() * sizeof(uint32_t), vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        buf_mem = quad.index_buffer->map(0, index_buffer.size() * sizeof(uint32_t), {});
        memcpy(buf_mem, index_buffer.data(), index_buffer.size() * sizeof(uint32_t));
        quad.index_buffer->unmap();
    }

    void Renderer::createAnimationResources()
    {
        //descriptor set layout
        vk::DescriptorSetLayoutBinding vertex_info_buffer;
        vertex_info_buffer.binding = 0;
        vertex_info_buffer.descriptorCount = 1;
        vertex_info_buffer.descriptorType = vk::DescriptorType::eStorageBuffer;
        vertex_info_buffer.pImmutableSamplers = nullptr;
        vertex_info_buffer.stageFlags = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutBinding skeleton_info_buffer;
        skeleton_info_buffer.binding = 1;
        skeleton_info_buffer.descriptorCount = 1;
        skeleton_info_buffer.descriptorType = vk::DescriptorType::eStorageBuffer;
        skeleton_info_buffer.pImmutableSamplers = nullptr;
        skeleton_info_buffer.stageFlags = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutBinding vertex_out_buffer;
        vertex_out_buffer.binding = 2;
        vertex_out_buffer.descriptorCount = 1;
        vertex_out_buffer.descriptorType = vk::DescriptorType::eStorageBuffer;
        vertex_out_buffer.pImmutableSamplers = nullptr;
        vertex_out_buffer.stageFlags = vk::ShaderStageFlagBits::eCompute;

        std::vector<vk::DescriptorSetLayoutBinding> descriptor_bindings = { vertex_info_buffer, skeleton_info_buffer, vertex_out_buffer };

        vk::DescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        layout_info.bindingCount = static_cast<uint32_t>(descriptor_bindings.size());
        layout_info.pBindings = descriptor_bindings.data();

        animation_descriptor_set_layout = device->createDescriptorSetLayoutUnique(layout_info, nullptr, dispatch);

        //pipeline layout
        vk::PipelineLayoutCreateInfo pipeline_layout_ci;
        std::vector<vk::DescriptorSetLayout> layouts = { *animation_descriptor_set_layout };
        pipeline_layout_ci.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipeline_layout_ci.pSetLayouts = layouts.data();

        vk::PushConstantRange push_constants;
        push_constants.size = sizeof(uint32_t);
        push_constants.offset = 0;
        push_constants.stageFlags = vk::ShaderStageFlagBits::eCompute;

        pipeline_layout_ci.pPushConstantRanges = &push_constants;
        pipeline_layout_ci.pushConstantRangeCount = 1;

        animation_pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_ci, nullptr, dispatch);

        //pipeline
        vk::ComputePipelineCreateInfo pipeline_ci;
        pipeline_ci.layout = *animation_pipeline_layout;

        auto animation_module = getShader("shaders/animation_skin.spv");

        vk::PipelineShaderStageCreateInfo animation_shader_stage_info;
        animation_shader_stage_info.stage = vk::ShaderStageFlagBits::eCompute;
        animation_shader_stage_info.module = *animation_module;
        animation_shader_stage_info.pName = "main";

        pipeline_ci.stage = animation_shader_stage_info;

        animation_pipeline = device->createComputePipelineUnique(nullptr, pipeline_ci, nullptr, dispatch);
    }

    void Renderer::recreateRenderer()
    {
        device->waitIdle(dispatch);
        swapchain_image_views.clear();
        swapchain_images.clear();

        createSwapchain();
        createRenderpasses();
        createDepthImage();
        //can skip this if scissor/viewport are dynamic
        createGraphicsPipeline();
        createFramebuffers();
        createGBufferResources();
        createDeferredCommandBuffer();
        createRayTracingResources();
        //recreate command buffers
        recreateStaticCommandBuffers();
    }

    void Renderer::recreateStaticCommandBuffers()
    {
        engine->game->scene->forEachEntity([this](std::shared_ptr<Entity>& entity)
        {
            auto work = entity->recreate_command_buffers(entity);
            if (work)
                engine->worker_pool.addWork(std::move(work));
        });
    }

    vk::UniqueHandle<vk::ShaderModule, vk::DispatchLoaderDynamic> Renderer::getShader(const std::string& file_name)
    {
        std::ifstream file(file_name, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        vk::ShaderModuleCreateInfo create_info;
        create_info.codeSize = buffer.size();
        create_info.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

        return device->createShaderModuleUnique(create_info, nullptr, dispatch);
    }

    bool Renderer::extensionsSupported(vk::PhysicalDevice device)
    {
        auto supported_extensions = device.enumerateDeviceExtensionProperties(nullptr, dispatch_static);

        std::set<std::string> requested_extensions{ device_extensions.begin(), device_extensions.end() };

        for (const auto& supported_extension : supported_extensions)
        {
            requested_extensions.erase(std::string(supported_extension.extensionName));
        }
        return requested_extensions.empty();
    }

    Renderer::swapChainInfo Renderer::getSwapChainInfo(vk::PhysicalDevice device) const
    {
        return {device.getSurfaceCapabilitiesKHR(surface, dispatch_static),
            device.getSurfaceFormatsKHR(surface, dispatch_static),
            device.getSurfacePresentModesKHR(surface, dispatch_static)
        };
    }

    vk::CommandBuffer Renderer::getRenderCommandbuffer(uint32_t image_index)
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = 1;

        auto buffer = device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, dispatch);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        buffer[0]->begin(begin_info, dispatch);

        vk::RenderPassBeginInfo renderpass_info = {};
        renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };

        if (RasterizationEnabled())
        {
            if (render_mode == RenderMode::Rasterization)
            {
                renderpass_info.renderPass = *shadowmap_render_pass;
                renderpass_info.renderArea.extent = vk::Extent2D{ shadowmap_dimension, shadowmap_dimension };

                std::array<vk::ClearValue, 1> clearValue = {};
                clearValue[0].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

                renderpass_info.clearValueCount = static_cast<uint32_t>(clearValue.size());
                renderpass_info.pClearValues = clearValue.data();

                auto shadowmap_buffers = engine->worker_pool.getShadowmapGraphicsBuffers(image_index);

                for (uint32_t i = 0; i < shadowmap_cascades; ++i)
                {
                    renderpass_info.framebuffer = *cascades[i].shadowmap_frame_buffer;
                    buffer[0]->pushConstants<uint32_t>(*shadowmap_pipeline_layout, vk::ShaderStageFlagBits::eVertex, sizeof(uint32_t), i, dispatch);
                    buffer[0]->beginRenderPass(renderpass_info, vk::SubpassContents::eSecondaryCommandBuffers, dispatch);
                    buffer[0]->executeCommands(shadowmap_buffers, dispatch);
                    buffer[0]->endRenderPass(dispatch);
                }
            }

            renderpass_info.renderPass = *gbuffer_render_pass;
            renderpass_info.framebuffer = *gbuffer.frame_buffer;
            std::array<vk::ClearValue, 7> clearValues = {};
            clearValues[0].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
            clearValues[1].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
            clearValues[2].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
            clearValues[3].color = std::array<float, 4>{ 0.2f, 0.4f, 0.6f, 1.0f };
            clearValues[4].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
            clearValues[5].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
            clearValues[6].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

            renderpass_info.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderpass_info.pClearValues = clearValues.data();
            renderpass_info.renderArea.extent = swapchain_extent;

            buffer[0]->beginRenderPass(renderpass_info, vk::SubpassContents::eSecondaryCommandBuffers, dispatch);
            auto secondary_buffers = engine->worker_pool.getSecondaryGraphicsBuffers(image_index);
            if (!secondary_buffers.empty())
                buffer[0]->executeCommands(secondary_buffers, dispatch);
            buffer[0]->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
            auto particle_buffers = engine->worker_pool.getParticleGraphicsBuffers(image_index);
            if (!particle_buffers.empty())
                buffer[0]->executeCommands(particle_buffers, dispatch);
            buffer[0]->endRenderPass(dispatch);
        }
        if (render_mode == RenderMode::Raytrace)
        {
            buffer[0]->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline, dispatch);

            buffer[0]->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline_layout, 0, *rtx_descriptor_sets_const[image_index], {}, dispatch);

            vk::WriteDescriptorSet write_info_target_albedo;
            write_info_target_albedo.descriptorCount = 1;
            write_info_target_albedo.descriptorType = vk::DescriptorType::eStorageImage;
            write_info_target_albedo.dstBinding = 0;
            write_info_target_albedo.dstArrayElement = 0;
            vk::DescriptorImageInfo target_image_info_albedo;
            target_image_info_albedo.imageView = *rtx_gbuffer.albedo.image_view;
            target_image_info_albedo.imageLayout = vk::ImageLayout::eGeneral;
            write_info_target_albedo.pImageInfo = &target_image_info_albedo;

            vk::WriteDescriptorSet write_info_target_light;
            write_info_target_light.descriptorCount = 1;
            write_info_target_light.descriptorType = vk::DescriptorType::eStorageImage;
            write_info_target_light.dstBinding = 1;
            write_info_target_light.dstArrayElement = 0;
            vk::DescriptorImageInfo target_image_info_light;
            target_image_info_light.imageView = *rtx_gbuffer.light.image_view;
            target_image_info_light.imageLayout = vk::ImageLayout::eGeneral;
            write_info_target_light.pImageInfo = &target_image_info_light;

            vk::DescriptorBufferInfo cam_buffer_info;
            cam_buffer_info.buffer = engine->camera->view_proj_ubo->buffer;
            cam_buffer_info.offset = uniform_buffer_align_up(sizeof(Camera::CameraData)) * getCurrentImage();
            cam_buffer_info.range = sizeof(Camera::CameraData);

            vk::WriteDescriptorSet write_info_cam;
            write_info_cam.descriptorCount = 1;
            write_info_cam.descriptorType = vk::DescriptorType::eUniformBuffer;
            write_info_cam.dstBinding = 2;
            write_info_cam.dstArrayElement = 0;
            write_info_cam.pBufferInfo = &cam_buffer_info;

            vk::DescriptorBufferInfo light_buffer_info_global;
            light_buffer_info_global.buffer = engine->lights.light_buffer->buffer;
            light_buffer_info_global.offset = getCurrentImage() * uniform_buffer_align_up(sizeof(engine->lights.light));
            light_buffer_info_global.range = sizeof(engine->lights.light);

            vk::WriteDescriptorSet write_info_light;
            write_info_light.descriptorCount = 1;
            write_info_light.descriptorType = vk::DescriptorType::eUniformBuffer;
            write_info_light.dstBinding = 3;
            write_info_light.dstArrayElement = 0;
            write_info_light.pBufferInfo = &light_buffer_info_global;

            buffer[0]->pushDescriptorSetKHR(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline_layout, 1,
                { write_info_target_albedo, write_info_target_light, write_info_cam, write_info_light }, dispatch);

            buffer[0]->traceRaysKHR(raygenSBT, missSBT, hitSBT, {}, swapchain_extent.width, swapchain_extent.height, 1, dispatch);
        }
        else if (render_mode == RenderMode::Hybrid)
        {
            buffer[0]->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline, dispatch);

            buffer[0]->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline_layout, 0, *rtx_descriptor_sets_const[image_index], {}, dispatch);

            vk::WriteDescriptorSet write_info_target_light;
            write_info_target_light.descriptorCount = 1;
            write_info_target_light.descriptorType = vk::DescriptorType::eStorageImage;
            write_info_target_light.dstBinding = 0;
            write_info_target_light.dstArrayElement = 0;

            vk::DescriptorImageInfo target_image_info_light;
            target_image_info_light.imageView = *rtx_gbuffer.light.image_view;
            target_image_info_light.imageLayout = vk::ImageLayout::eGeneral;
            write_info_target_light.pImageInfo = &target_image_info_light;

            vk::DescriptorBufferInfo light_buffer_info;
            light_buffer_info.buffer = engine->lights.light_buffer->buffer;
            light_buffer_info.offset = getCurrentImage() * uniform_buffer_align_up(sizeof(engine->lights.light));
            light_buffer_info.range = sizeof(engine->lights.light);

            vk::WriteDescriptorSet write_info_light;
            write_info_light.descriptorCount = 1;
            write_info_light.descriptorType = vk::DescriptorType::eUniformBuffer;
            write_info_light.dstBinding = 3;
            write_info_light.dstArrayElement = 0;
            write_info_light.pBufferInfo = &light_buffer_info;

            vk::DescriptorImageInfo position_info;
            position_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            position_info.imageView = *gbuffer.position.image_view;
            position_info.sampler = *gbuffer.sampler;

            vk::WriteDescriptorSet write_info_position;
            write_info_position.descriptorCount = 1;
            write_info_position.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            write_info_position.dstBinding = 4;
            write_info_position.dstArrayElement = 0;
            write_info_position.pImageInfo = &position_info;

            vk::DescriptorImageInfo normal_info;
            normal_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            normal_info.imageView = *gbuffer.normal.image_view;
            normal_info.sampler = *gbuffer.sampler;

            vk::WriteDescriptorSet write_info_normal;
            write_info_normal.descriptorCount = 1;
            write_info_normal.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            write_info_normal.dstBinding = 5;
            write_info_normal.dstArrayElement = 0;
            write_info_normal.pImageInfo = &normal_info;

            vk::DescriptorImageInfo face_normal_info;
            face_normal_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            face_normal_info.imageView = *gbuffer.face_normal.image_view;
            face_normal_info.sampler = *gbuffer.sampler;

            vk::WriteDescriptorSet write_info_face_normal;
            write_info_face_normal.descriptorCount = 1;
            write_info_face_normal.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            write_info_face_normal.dstBinding = 6;
            write_info_face_normal.dstArrayElement = 0;
            write_info_face_normal.pImageInfo = &face_normal_info;

            vk::DescriptorImageInfo material_index_info;
            material_index_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            material_index_info.imageView = *gbuffer.material.image_view;
            material_index_info.sampler = *gbuffer.sampler;

            vk::WriteDescriptorSet write_info_material_index;
            write_info_material_index.descriptorCount = 1;
            write_info_material_index.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            write_info_material_index.dstBinding = 7;
            write_info_material_index.dstArrayElement = 0;
            write_info_material_index.pImageInfo = &material_index_info;

            buffer[0]->pushDescriptorSetKHR(vk::PipelineBindPoint::eRayTracingKHR, *rtx_pipeline_layout, 1, { write_info_target_light, write_info_light, write_info_position, write_info_normal, write_info_face_normal, write_info_material_index }, dispatch);

            buffer[0]->traceRaysKHR(raygenSBT, missSBT, hitSBT, {}, swapchain_extent.width, swapchain_extent.height, 1, dispatch);
        }

        buffer[0]->end(dispatch);
        render_commandbuffers[image_index] = std::move(buffer[0]);
        return *render_commandbuffers[image_index];
    }

    bool Renderer::checkValidationLayerSupport() const
    {
        auto availableLayers = vk::enumerateInstanceLayerProperties(dispatch_static);

        for (const char* layerName : validation_layers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }
        return true;
    }

    std::vector<const char*> Renderer::getRequiredExtensions() const
    {
        uint32_t extensionCount = 0;

        SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);

        std::vector<const char*> extensions(extensionCount);

        SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        extensions.insert(extensions.end(), instance_extensions.begin(), instance_extensions.end());

        return extensions;
    }

    std::tuple<std::optional<uint32_t>, std::optional<std::uint32_t>, std::optional<std::uint32_t>> Renderer::getQueueFamilies(vk::PhysicalDevice device) const
    {
        auto queue_families = device.getQueueFamilyProperties(dispatch_static);
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

            if (device.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface, dispatch_static) && family.queueCount > 0 && !present)
            {
                present = static_cast<uint32_t>(i);
            }
        }
        if (compute_dedicated)
            compute = compute_dedicated;
        return { graphics, present, compute };
    }

    size_t lotus::Renderer::uniform_buffer_align_up(size_t in_size) const
    {
        return align_up(in_size, properties.properties.limits.minUniformBufferOffsetAlignment);
    }

    size_t lotus::Renderer::storage_buffer_align_up(size_t in_size) const
    {
        return align_up(in_size, properties.properties.limits.minStorageBufferOffsetAlignment);
    }

    size_t lotus::Renderer::align_up(size_t in_size, size_t alignment) const
    {
        if (in_size % alignment == 0)
            return in_size;
        else
            return ((in_size / alignment) + 1) * alignment;
    }

    void Renderer::drawFrame()
    {
        if (!engine->game || !engine->game->scene)
            return;

        engine->worker_pool.deleteFinished();
        device->waitForFences(*frame_fences[current_frame], true, std::numeric_limits<uint64_t>::max(), dispatch);

        auto prev_image = current_image;
        auto [result, value] = device->acquireNextImageKHR(*swapchain, std::numeric_limits<uint64_t>::max(), *image_ready_sem[current_frame], nullptr, dispatch);
        current_image = value;

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            recreateRenderer();
            return;
        }
        engine->worker_pool.clearProcessed(current_image);
        if (old_swapchain && old_swapchain_image == current_image)
        {
            old_swapchain.reset();
        }
        engine->worker_pool.waitIdle();
        if (raytracer->hasQueries())
        {
            raytracer->runQueries(prev_image);
        }
        engine->game->scene->render();

        engine->worker_pool.waitIdle();
        engine->worker_pool.startProcessing(current_image);
        engine->lights.UpdateLightBuffer();

        std::vector<vk::Semaphore> waitSemaphores = { *image_ready_sem[current_frame]};
        std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eRayTracingShaderKHR };
        auto buffers = engine->worker_pool.getPrimaryComputeBuffers(current_image);
        if (!buffers.empty())
        {
            vk::SubmitInfo submitInfo = {};
            submitInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());
            submitInfo.pCommandBuffers = buffers.data();
            //TODO: make this more fine-grained (having all graphics wait for compute is overkill)
            vk::Semaphore compute_signal_sems[] = { *compute_sem };
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = compute_signal_sems;

            compute_queue.submit(submitInfo, nullptr, dispatch);
            waitSemaphores.push_back(*compute_sem);
            waitStages.push_back(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits::eVertexInput);
        }

        vk::SubmitInfo submitInfo = {};
        submitInfo.waitSemaphoreCount = waitSemaphores.size();
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();

        buffers = engine->worker_pool.getPrimaryGraphicsBuffers(current_image);
        buffers.push_back(getRenderCommandbuffer(current_image));

        submitInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());
        submitInfo.pCommandBuffers = buffers.data();

        vk::Semaphore gbuffer_semaphores[] = { *gbuffer_sem };
        submitInfo.pSignalSemaphores = gbuffer_semaphores;
        submitInfo.signalSemaphoreCount = 1;

        graphics_queue.submit(submitInfo, nullptr, dispatch);

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = gbuffer_semaphores;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &*deferred_command_buffers[current_image];

        vk::Semaphore signalSemaphores[] = { *frame_finish_sem[current_frame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        device->resetFences(*frame_fences[current_frame]);

        graphics_queue.submit(submitInfo, *frame_fences[current_frame], dispatch);

        vk::PresentInfoKHR presentInfo = {};

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        vk::SwapchainKHR swap_chains[] = {*swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swap_chains;

        presentInfo.pImageIndices = &current_image;

        try
        {
            present_queue.presentKHR(presentInfo, dispatch);
        }
        catch (vk::OutOfDateKHRError& )
        {
            resize = false;
            recreateRenderer();
        }

        if (resize)
        {
            resize = false;
            recreateRenderer();
        }

        current_frame = (current_frame + 1) % max_pending_frames;
    }

    vk::Format Renderer::getDepthFormat() const
    {
        for (vk::Format format : {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint})
        {
            vk::FormatProperties props = physical_device.getFormatProperties(format, dispatch);

            if ((props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) == vk::FormatFeatureFlagBits::eDepthStencilAttachment)
            {
                return format;
            }
        }
        throw std::runtime_error("Unable to find supported depth format");
    }
}
