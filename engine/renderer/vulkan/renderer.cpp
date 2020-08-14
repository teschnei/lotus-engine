#include "renderer.h"
#include <glm/glm.hpp>
#include <fstream>
#include <iostream>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/config.h"
#include "engine/entity/camera.h"
#include "engine/entity/renderable_entity.h"

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
        gpu = std::make_unique<GPU>(*instance, *surface, engine->config.get(), validation_layers);

        if (enableValidationLayers)
        {
            vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
            debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
            debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
            debugCreateInfo.pfnUserCallback = debugCallback;
            debug_messenger = instance->createDebugUtilsMessengerEXTUnique(debugCreateInfo);
        }

        createSwapchain();
    }

    Renderer::~Renderer()
    {
        gpu->device->waitIdle();
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
        appInfo.apiVersion = VK_API_VERSION_1_2;

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
        vk::CommandPoolCreateInfo pool_info = {};
        pool_info.queueFamilyIndex = gpu->graphics_queue_index;

        command_pool = gpu->device->createCommandPoolUnique(pool_info, nullptr);
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

        quad.vertex_buffer = gpu->memory_manager->GetBuffer(vertex_buffer.size() * sizeof(Vertex), vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
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

        quad.index_buffer = gpu->memory_manager->GetBuffer(index_buffer.size() * sizeof(uint32_t), vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
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

    void Renderer::resizeRenderer()
    {
        recreateRenderer();
        if (engine->camera)
        {
            engine->camera->setPerspective(glm::radians(70.f), swapchain->extent.width / (float)swapchain->extent.height, 0.01f, 1000.f);
        }
    }

    void Renderer::recreateRenderer()
    {
        gpu->device->waitIdle();
        engine->worker_pool.reset();
        swapchain->recreateSwapchain(current_image);

        createAnimationResources();
        //recreate command buffers
        recreateStaticCommandBuffers();
    }

    void Renderer::recreateStaticCommandBuffers()
    {
        //delete all the existing buffers first, single-threadedly (the destructor uses the pool it was created on)
        engine->game->scene->forEachEntity([this](std::shared_ptr<Entity>& entity)
        {
            if (auto ren = std::dynamic_pointer_cast<RenderableEntity>(entity))
            {
                ren->command_buffers.clear();
                ren->shadowmap_buffers.clear();
            }
        });
        engine->game->scene->forEachEntity([this](std::shared_ptr<Entity>& entity)
        {
            auto work = entity->recreate_command_buffers(entity);
            if (work)
                engine->worker_pool->addWork(std::move(work));
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
