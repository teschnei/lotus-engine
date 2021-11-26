#include "renderer.h"
#include <glm/glm.hpp>
#include <fstream>
#include <iostream>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/config.h"

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

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    Renderer::Renderer(Engine* _engine) : engine(_engine)
    {
        window = std::make_unique<Window>(&engine->settings, engine->config.get());

        vk::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr instance_proc_addr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance_proc_addr);
        createInstance(engine->settings.app_name, engine->settings.app_version);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

        surface = window->createSurface(*instance);

        if (enableValidationLayers)
        {
            gpu = std::make_unique<GPU>(*instance, *surface, engine->config.get(), validation_layers);
            vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
            debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
            debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
            debugCreateInfo.pfnUserCallback = debugCallback;
            debug_messenger = instance->createDebugUtilsMessengerEXTUnique(debugCreateInfo);
        }
        else
        {
            gpu = std::make_unique<GPU>(*instance, *surface, engine->config.get(), std::vector<const char*>());
        }

        createSwapchain();
        resources = std::make_unique<GlobalResources>(engine, this);
        post_process = std::make_unique<PostProcessPipeline>(this);
    }

    Renderer::~Renderer()
    {
        gpu->device->waitIdle();
    }

    Task<> Renderer::InitCommon()
    {
        raytrace_queryer = std::make_unique<RaytraceQueryer>(engine);
        ui = std::make_unique<UiRenderer>(engine, this);
        co_await ui->Init();
    }

    void Renderer::createInstance(const std::string& app_name, uint32_t app_version)
    {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        vk::ApplicationInfo appInfo {
            .pApplicationName = app_name.c_str(),
            .applicationVersion = app_version,
            .pEngineName = "lotus",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_2
        };

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
            createInfo.pNext = &debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        instance = vk::createInstanceUnique(createInfo, nullptr);
    }

    void Renderer::createSwapchain()
    {
        swapchain = std::make_unique<Swapchain>(engine->config.get(), gpu.get(), window.get(), *surface);
    }

    void Renderer::createCommandPool()
    {
        command_pool = gpu->createCommandPool(GPU::QueueType::Graphics);
        local_compute_pool = gpu->createCommandPool(GPU::QueueType::Compute);
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

        animation_descriptor_set_layout = gpu->device->createDescriptorSetLayoutUnique(layout_info, nullptr);

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

        animation_pipeline_layout = gpu->device->createPipelineLayoutUnique(pipeline_layout_ci, nullptr);

        //pipeline
        vk::ComputePipelineCreateInfo pipeline_ci;
        pipeline_ci.layout = *animation_pipeline_layout;

        auto animation_module = getShader("shaders/animation_skin.spv");

        vk::PipelineShaderStageCreateInfo animation_shader_stage_info;
        animation_shader_stage_info.stage = vk::ShaderStageFlagBits::eCompute;
        animation_shader_stage_info.module = *animation_module;
        animation_shader_stage_info.pName = "main";

        pipeline_ci.stage = animation_shader_stage_info;

        animation_pipeline = gpu->device->createComputePipelineUnique(nullptr, pipeline_ci, nullptr);
    }

    Task<> Renderer::resizeRenderer()
    {
        //if (auto [x, y] = window->getWindowDimensions(); x != 0 && y != 0)
        if(!window->isMinimized())
        {
            co_await recreateRenderer();
            if (engine->camera)
            {
                engine->camera->setPerspective(glm::radians(70.f), swapchain->extent.width / (float)swapchain->extent.height, 0.01f, 1000.f);
            }
        }
    }

    Task<> Renderer::recreateStaticCommandBuffers()
    {
        /*
        //delete all the existing buffers first, single-threadedly (the destructor uses the pool it was created on)
        //TODO: generalize this? only baked commands need to be cleared here, and either no commands should be baked or just landscape should be
        engine->game->scene->forEachEntity([](std::shared_ptr<Entity>& entity)
        {
            if (auto ren = std::dynamic_pointer_cast<LandscapeEntity>(entity))
            {
                ren->command_buffers.clear();
                ren->shadowmap_buffers.clear();
            }
        });
        std::vector<WorkerTask<>> tasks;
        engine->game->scene->forEachEntity([this, &tasks](std::shared_ptr<Entity>& entity)
        {
            tasks.push_back(entity->ReInitWork());
        });
        for(auto& task : tasks)
        {
            co_await task;
        }
        */
        co_return;
    }

    Renderer::ThreadLocals Renderer::createThreadLocals()
    {
        graphics_pool = gpu->createCommandPool(GPU::QueueType::Graphics);
        compute_pool = gpu->createCommandPool(GPU::QueueType::Compute);

        std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
        poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(getFrameCount());
        poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(getFrameCount());

        vk::DescriptorPoolCreateInfo poolInfo = {};
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(getFrameCount());
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

        desc_pool = gpu->device->createDescriptorPoolUnique(poolInfo);

        return ThreadLocals{this};
    }

    void Renderer::deleteThreadLocals()
    {
        //move static thread_locals in renderer to the renderer's storage so it can be destructed in the right order
        std::lock_guard lk{ engine->renderer->shutdown_mutex };
        engine->renderer->shutdown_command_pools.push_back(std::move(engine->renderer->graphics_pool));
        engine->renderer->shutdown_command_pools.push_back(std::move(engine->renderer->compute_pool));
        engine->renderer->shutdown_descriptor_pools.push_back(std::move(engine->renderer->desc_pool));
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

        return gpu->device->createShaderModuleUnique(create_info, nullptr);
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
        auto extensions = window->getRequiredExtensions();
        
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        extensions.insert(extensions.end(), instance_extensions.begin(), instance_extensions.end());

        return extensions;
    }

    size_t lotus::Renderer::uniform_buffer_align_up(size_t in_size) const
    {
        return align_up(in_size, gpu->properties.properties.limits.minUniformBufferOffsetAlignment);
    }

    size_t lotus::Renderer::storage_buffer_align_up(size_t in_size) const
    {
        return align_up(in_size, gpu->properties.properties.limits.minStorageBufferOffsetAlignment);
    }

    size_t lotus::Renderer::align_up(size_t in_size, size_t alignment) const
    {
        if (in_size % alignment == 0)
            return in_size;
        else
            return ((in_size / alignment) + 1) * alignment;
    }
}
