#include "renderer.h"
#include <SDL_vulkan.h>
#include <glm/glm.hpp>
#include <fstream>
#include <iostream>

#include "core.h"
#include "game.h"
#include "task/entity_render.h"
#include "../../ffxi/mmb.h"

constexpr size_t WIDTH = 1900;
constexpr size_t HEIGHT = 1000;

constexpr size_t shadowmap_dimension = 4096;

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

    Renderer::Renderer(Engine* _engine, const std::string& app_name, uint32_t app_version) : engine(_engine)
    {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow(app_name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        createInstance(app_name, app_version);
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

        render_commandbuffers.resize(getImageCount());
    }

    Renderer::~Renderer()
    {
        device->waitIdle(dispatch);
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

        instance = vk::createInstanceUnique(createInfo);

        if (enableValidationLayers)
        {
            if (CreateDebugUtilsMessengerEXT(*instance, &debugCreateInfo, nullptr, &debug_messenger) != VK_SUCCESS) {
                throw std::runtime_error("failed to set up debug messenger!");
            }
        }
    }

    void Renderer::createPhysicalDevice()
    {
        auto physical_devices = instance->enumeratePhysicalDevices();

        physical_device = *std::find_if(physical_devices.begin(), physical_devices.end(), [this](auto& device) {
            auto [graphics, present] = getQueueFamilies(device);
            auto extensions_supported = extensionsSupported(device);
            auto swap_chain_info = getSwapChainInfo(device);
            auto supported_features = device.getFeatures();
            return graphics && present && extensions_supported && !swap_chain_info.formats.empty() && !swap_chain_info.present_modes.empty() && supported_features.samplerAnisotropy;
        });

        if (!physical_device)
        {
            throw std::runtime_error("Unable to find a suitable Vulkan GPU");
        }
    }

    void Renderer::createDevice()
    {
        auto [graphics_queue_idx, present_queue_idx] = getQueueFamilies(physical_device);
        //deduplicate queues
        std::set<uint32_t> queues = { graphics_queue_idx.value(), present_queue_idx.value() };

        std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
        float queue_priority = 1.f;

        for (auto queue : queues)
        {
            vk::DeviceQueueCreateInfo create_info;
            create_info.queueFamilyIndex = queue;
            create_info.pQueuePriorities = &queue_priority;
            create_info.queueCount = 1;
            queue_create_infos.push_back(create_info);
        }

        vk::PhysicalDeviceFeatures physical_device_features;
        physical_device_features.samplerAnisotropy = VK_TRUE;
        physical_device_features.depthClamp = VK_TRUE;

        vk::DeviceCreateInfo device_create_info;
        device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create_info.pQueueCreateInfos = queue_create_infos.data();
        device_create_info.pEnabledFeatures = &physical_device_features;
        device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
        device_create_info.ppEnabledExtensionNames = device_extensions.data();

        if (enableValidationLayers) {
            device_create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            device_create_info.ppEnabledLayerNames = validation_layers.data();
        } else {
            device_create_info.enabledLayerCount = 0;
        }
        device = physical_device.createDeviceUnique(device_create_info);

        graphics_queue = device->getQueue(graphics_queue_idx.value(), 0);
        present_queue = device->getQueue(present_queue_idx.value(), 0);

        dispatch = vk::DispatchLoaderDynamic(*instance, *device);
    }

    void Renderer::createSwapchain()
    {
        //device->waitIdle(dispatch);
        //swapchain_image_views.clear();
        //swapchain_images.clear();
        //swapchain.reset();
        if (swapchain)
        {
            old_swapchain = std::move(swapchain);
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

            swap_extent =
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

        auto [graphics, present] = getQueueFamilies(physical_device);
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

        std::array<vk::AttachmentDescription, 2> attachments = {color_attachment, depth_attachment};
        vk::RenderPassCreateInfo render_pass_info;
        render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        render_pass_info.pAttachments = attachments.data();
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        render_pass = device->createRenderPassUnique(render_pass_info, nullptr, dispatch);

        depth_attachment.storeOp = vk::AttachmentStoreOp::eStore;
        depth_attachment_ref.attachment = 0;

        std::array<vk::AttachmentDescription, 1> shadow_attachments = {depth_attachment};
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

        shadowmap_render_pass = device->createRenderPassUnique(render_pass_info, nullptr, dispatch);
    }

    void Renderer::createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding ubo_layout_binding;
        ubo_layout_binding.binding = 0;
        ubo_layout_binding.descriptorCount = 1;
        ubo_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        ubo_layout_binding.pImmutableSamplers = nullptr;
        ubo_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutBinding shadowmap_layout_binding;
        shadowmap_layout_binding.binding = 1;
        shadowmap_layout_binding.descriptorCount = 1;
        shadowmap_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        shadowmap_layout_binding.pImmutableSamplers = nullptr;
        shadowmap_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding sample_layout_binding;
        sample_layout_binding.binding = 2;
        sample_layout_binding.descriptorCount = 1;
        sample_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        sample_layout_binding.pImmutableSamplers = nullptr;
        sample_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding lightsource_layout_binding;
        lightsource_layout_binding.binding = 3;
        lightsource_layout_binding.descriptorCount = 1;
        lightsource_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        lightsource_layout_binding.pImmutableSamplers = nullptr;
        lightsource_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        std::array<vk::DescriptorSetLayoutBinding, 4> static_bindings = { ubo_layout_binding, shadowmap_layout_binding, sample_layout_binding, lightsource_layout_binding };

        vk::DescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
        layout_info.bindingCount = static_cast<uint32_t>(static_bindings.size());
        layout_info.pBindings = static_bindings.data();

        static_descriptor_set_layout = device->createDescriptorSetLayoutUnique(layout_info, nullptr, dispatch);


        //std::array<vk::DescriptorSetLayoutBinding, 1> material_bindings = {sample_layout_binding};
        //layout_info.bindingCount = static_cast<uint32_t>(material_bindings.size());
        //layout_info.pBindings = material_bindings.data();

        //material_descriptor_set_layout = device->createDescriptorSetLayoutUnique(layout_info, nullptr, dispatch);

        vk::DescriptorSetLayoutBinding cascade_matrices;
        cascade_matrices.binding = 1;
        cascade_matrices.descriptorCount = 1;
        cascade_matrices.descriptorType = vk::DescriptorType::eUniformBuffer;
        cascade_matrices.pImmutableSamplers = nullptr;
        cascade_matrices.stageFlags = vk::ShaderStageFlagBits::eVertex;

        std::array<vk::DescriptorSetLayoutBinding, 3> shadowmap_bindings = {ubo_layout_binding, cascade_matrices, sample_layout_binding};

        layout_info.bindingCount = static_cast<uint32_t>(shadowmap_bindings.size());
        layout_info.pBindings = shadowmap_bindings.data();

        shadowmap_descriptor_set_layout = device->createDescriptorSetLayoutUnique(layout_info, nullptr, dispatch);
    }

    void Renderer::createGraphicsPipeline()
    {
        auto vertex_module = getShader("shaders/vert.spv");
        auto fragment_module = getShader("shaders/frag.spv");

        vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
        vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
        vert_shader_stage_info.module = *vertex_module;
        vert_shader_stage_info.pName = "main";

        vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
        frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
        frag_shader_stage_info.module = *fragment_module;
        frag_shader_stage_info.pName = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {vert_shader_stage_info, frag_shader_stage_info};

        vk::PipelineVertexInputStateCreateInfo vertex_input_info;

        auto binding_descriptions = FFXI::MMB::Vertex::getBindingDescriptions();
        auto attribute_descriptions = FFXI::MMB::Vertex::getAttributeDescriptions();

        vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_descriptions.size());
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexBindingDescriptions = binding_descriptions.data();
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

        vk::PipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.topology = vk::PrimitiveTopology::eTriangleStrip;
        input_assembly.primitiveRestartEnable = false;

        vk::Viewport viewport;
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchain_extent.width;
        viewport.height = (float)swapchain_extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor;
        scissor.offset = {0, 0};
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
        color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;

        vk::PipelineColorBlendStateCreateInfo color_blending;
        color_blending.logicOpEnable = false;
        color_blending.logicOp = vk::LogicOp::eCopy;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;
        color_blending.blendConstants[0] = 0.0f;
        color_blending.blendConstants[1] = 0.0f;
        color_blending.blendConstants[2] = 0.0f;
        color_blending.blendConstants[3] = 0.0f;

        std::array<vk::DescriptorSetLayout, 1> descriptor_layouts = { *static_descriptor_set_layout };

        vk::PipelineLayoutCreateInfo pipeline_layout_info;
        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_layouts.size());
        pipeline_layout_info.pSetLayouts = descriptor_layouts.data();

        pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_info, nullptr, dispatch);

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
        pipeline_info.layout = *pipeline_layout;
        pipeline_info.renderPass = *render_pass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = nullptr;

        main_graphics_pipeline = device->createGraphicsPipelineUnique(nullptr, pipeline_info, nullptr, dispatch);

        fragment_module = getShader("shaders/frag_blend.spv");

        frag_shader_stage_info.module = *fragment_module;

        shaderStages[1] = frag_shader_stage_info;

        blended_graphics_pipeline = device->createGraphicsPipelineUnique(nullptr, pipeline_info, nullptr, dispatch);

        vk::PushConstantRange push_constant_range;
        push_constant_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
        push_constant_range.size = 4;
        push_constant_range.offset = 0;

        color_blending.attachmentCount = 0;

        std::array<vk::DescriptorSetLayout, 1> shadowmap_descriptor_layouts = { *shadowmap_descriptor_set_layout };

        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(shadowmap_descriptor_layouts.size());
        pipeline_layout_info.pSetLayouts = shadowmap_descriptor_layouts.data();
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &push_constant_range;

        shadowmap_pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_info, nullptr, dispatch);

        pipeline_info.layout = *shadowmap_pipeline_layout;

        pipeline_info.renderPass = *shadowmap_render_pass;
        vertex_module = getShader("shaders/vert_shadow.spv");
        fragment_module = getShader("shaders/frag_shadow.spv");

        vert_shader_stage_info.module = *vertex_module;
        frag_shader_stage_info.module = *fragment_module;

        shaderStages = {vert_shader_stage_info, frag_shader_stage_info};

        pipeline_info.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipeline_info.pStages = shaderStages.data();

        viewport.width = shadowmap_dimension;
        viewport.height = shadowmap_dimension;
        scissor.extent.width = shadowmap_dimension;
        scissor.extent.height = shadowmap_dimension;

        rasterizer.depthClampEnable = true;

        blended_shadowmap_pipeline = device->createGraphicsPipelineUnique(nullptr, pipeline_info, nullptr, dispatch);

        pipeline_info.stageCount = 1;
        pipeline_info.pStages = &vert_shader_stage_info;
        shadowmap_pipeline = device->createGraphicsPipelineUnique(nullptr, pipeline_info, nullptr, dispatch);
    }

    void Renderer::createDepthImage()
    {
        auto format = getDepthFormat();

        depth_image = memory_manager->GetImage(swapchain_extent.width, swapchain_extent.height, format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = format;
        image_view_info.image = *depth_image->image;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        depth_image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);
    }

    void Renderer::createFramebuffers()
    {
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
    }

    void Renderer::createCommandPool()
    {
        auto graphics_queue = getQueueFamilies(engine->renderer.physical_device).first;

        vk::CommandPoolCreateInfo pool_info = {};
        pool_info.queueFamilyIndex = graphics_queue.value();

        command_pool = device->createCommandPoolUnique(pool_info);
    }

    void Renderer::createShadowmapResources()
    {
        auto format = getDepthFormat();

        shadowmap_image = memory_manager->GetImage(shadowmap_dimension, shadowmap_dimension, format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, shadowmap_cascades);

        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = *shadowmap_image->image;
        image_view_info.viewType = vk::ImageViewType::e2DArray;
        image_view_info.format = format;
        image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = shadowmap_cascades;
        shadowmap_image_view = device->createImageViewUnique(image_view_info, nullptr, dispatch);

        for (size_t i = 0; i < shadowmap_cascades; ++i)
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
        auto supported_extensions = device.enumerateDeviceExtensionProperties();

        std::set<std::string> requested_extensions{ device_extensions.begin(), device_extensions.end() };

        for (const auto& supported_extension : supported_extensions)
        {
            requested_extensions.erase(supported_extension.extensionName);
        }
        return requested_extensions.empty();
    }

    Renderer::swapChainInfo Renderer::getSwapChainInfo(vk::PhysicalDevice device) const
    {
        return {device.getSurfaceCapabilitiesKHR(surface),
            device.getSurfaceFormatsKHR(surface),
            device.getSurfacePresentModesKHR(surface)
        };
    }

    vk::CommandBuffer Renderer::getRenderCommandbuffer(uint32_t image_index)
    {
        vk::CommandBufferAllocateInfo alloc_info = {};
        alloc_info.commandPool = *command_pool;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = 1;

        auto buffer = engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, dispatch);

        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        buffer[0]->begin(begin_info, dispatch);

        vk::RenderPassBeginInfo renderpass_info = {};
        renderpass_info.renderPass = *shadowmap_render_pass;
        renderpass_info.renderArea.offset = { 0, 0 };
        renderpass_info.renderArea.extent = { shadowmap_dimension, shadowmap_dimension };

        std::array<vk::ClearValue, 1> clearValue = {};
        clearValue[0].depthStencil = { 1.0f, 0 };

        renderpass_info.clearValueCount = static_cast<uint32_t>(clearValue.size());
        renderpass_info.pClearValues = clearValue.data();

        auto shadowmap_buffers = engine->worker_pool.getShadowmapCommandBuffers(image_index);

        for (size_t i = 0; i < shadowmap_cascades; ++i)
        {
            renderpass_info.framebuffer = *cascades[i].shadowmap_frame_buffer;
            buffer[0]->pushConstants<uint32_t>(*shadowmap_pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, i, dispatch);
            buffer[0]->beginRenderPass(renderpass_info, vk::SubpassContents::eSecondaryCommandBuffers, dispatch);
            buffer[0]->executeCommands(shadowmap_buffers, dispatch);
            buffer[0]->endRenderPass(dispatch);
        }

        renderpass_info.renderPass = *render_pass;
        renderpass_info.framebuffer = *frame_buffers[image_index];
        std::array<vk::ClearValue, 2> clearValues = {};
        clearValues[0].color = std::array<float, 4>{ 0.2f, 0.4f, 0.6f, 1.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderpass_info.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderpass_info.pClearValues = clearValues.data();
        renderpass_info.renderArea.extent = swapchain_extent;

        buffer[0]->beginRenderPass(renderpass_info, vk::SubpassContents::eSecondaryCommandBuffers, dispatch);
        auto secondary_buffers = engine->worker_pool.getSecondaryCommandBuffers(image_index);
        buffer[0]->executeCommands(secondary_buffers, dispatch);
        buffer[0]->endRenderPass(dispatch);

        buffer[0]->end(dispatch);

        render_commandbuffers[image_index] = std::move(buffer[0]);
        return *render_commandbuffers[image_index];
    }

    bool Renderer::checkValidationLayerSupport() const
    {
        auto availableLayers = vk::enumerateInstanceLayerProperties();

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

    std::pair<std::optional<uint32_t>, std::optional<std::uint32_t>> Renderer::getQueueFamilies(vk::PhysicalDevice device) const
    {
        auto queue_families = device.getQueueFamilyProperties();
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        for (size_t i = 0; i < queue_families.size(); ++i)
        {
            auto& family = queue_families[i];
            if (family.queueFlags & vk::QueueFlagBits::eGraphics && family.queueCount > 0)
            {
                graphics = static_cast<uint32_t>(i);
            }

            if (device.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface) && family.queueCount > 0)
            {
                present = static_cast<uint32_t>(i);
            }
            if (graphics && present) break;
        }
        return { graphics, present };
    }

    void Renderer::drawFrame()
    {
        if (!engine->game || !engine->game->scene)
            return;

        engine->worker_pool.deleteFinished();
        device->waitForFences(*frame_fences[current_frame], true, std::numeric_limits<uint64_t>::max(), dispatch);

        try
        {
            current_image = device->acquireNextImageKHR(*swapchain, std::numeric_limits<uint64_t>::max(), *image_ready_sem[current_frame], nullptr, dispatch).value;
        }
        catch (const vk::OutOfDateKHRError&)
        {
            createSwapchain();
            return;
        }
        engine->worker_pool.clearProcessed(current_image);
        engine->game->scene->render(engine);

        engine->worker_pool.waitIdle();
        engine->worker_pool.startProcessing(current_image);

        vk::SubmitInfo submitInfo = {};

        vk::Semaphore waitSemaphores[] = { *image_ready_sem[current_frame] };
        vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        auto buffers = engine->worker_pool.getPrimaryCommandBuffers(current_image);
        buffers.push_back(getRenderCommandbuffer(current_image));

        submitInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());
        submitInfo.pCommandBuffers = buffers.data();

        vk::Semaphore signalSemaphores[] = {*frame_finish_sem[current_frame]};
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
            present_queue.presentKHR(presentInfo);
        }
        catch (const vk::OutOfDateKHRError&)
        {
            resize = false;
            createSwapchain();
        }

        if (resize)
        {
            resize = false;
            createSwapchain();
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
