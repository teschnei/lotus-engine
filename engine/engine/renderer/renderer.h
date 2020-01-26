#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <SDL.h>
#include "memory.h"
#include "raytrace_query.h"

namespace lotus
{
    class Engine;

    enum class RenderMode
    {
        Rasterization,
        Hybrid,
        RTX
    };

    class Renderer
    {
    public:
        struct Settings
        {
            std::vector<vk::VertexInputBindingDescription> landscape_vertex_input_binding_descriptions;
            std::vector<vk::VertexInputAttributeDescription> landscape_vertex_input_attribute_descriptions;
            std::vector<vk::VertexInputBindingDescription> model_vertex_input_binding_descriptions;
            std::vector<vk::VertexInputAttributeDescription> model_vertex_input_attribute_descriptions;
        };
        Renderer(Engine* engine);
        ~Renderer();

        void generateCommandBuffers();

        uint32_t getImageCount() const { return static_cast<uint32_t>(swapchain_images.size()); }
        uint32_t getCurrentImage() const { return current_image; }
        void setCurrentImage(int _current_image) { current_image = _current_image; }
        std::tuple<std::optional<uint32_t>, std::optional<std::uint32_t>, std::optional<uint32_t>> getQueueFamilies(vk::PhysicalDevice device) const;

        void drawFrame();
        void resized() { resize = true; }

        vk::UniqueHandle<vk::ShaderModule, vk::DispatchLoaderDynamic> getShader(const std::string& file_name);

        vk::UniqueHandle<vk::Instance, vk::DispatchLoaderStatic> instance;
        vk::PhysicalDevice physical_device;
        vk::UniqueHandle<vk::Device, vk::DispatchLoaderStatic> device;
        std::unique_ptr<MemoryManager> memory_manager;
        vk::Queue graphics_queue;
        vk::Queue present_queue;
        vk::Queue compute_queue;
        vk::UniqueHandle<vk::SwapchainKHR, vk::DispatchLoaderDynamic> swapchain;
        vk::UniqueHandle<vk::SwapchainKHR, vk::DispatchLoaderDynamic> old_swapchain;
        uint32_t old_swapchain_image{ 0 };
        vk::Extent2D swapchain_extent{};
        vk::Format swapchain_image_format{};
        std::vector<vk::Image> swapchain_images;
        std::vector<vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic>> swapchain_image_views;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> render_pass;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> shadowmap_render_pass;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> gbuffer_render_pass;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> static_descriptor_set_layout;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> shadowmap_descriptor_set_layout;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> deferred_descriptor_set_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> pipeline_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> shadowmap_pipeline_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> deferred_pipeline_layout;

        struct PipelineGroup
        {
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> graphics_pipeline;
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> blended_graphics_pipeline;
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> shadowmap_pipeline;
            vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> blended_shadowmap_pipeline;
        } landscape_pipeline_group, main_pipeline_group;

        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> deferred_pipeline;
        std::unique_ptr<Image> depth_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> depth_image_view;
        std::vector<vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic>> frame_buffers;
        SDL_Window* window {nullptr};
        vk::SurfaceKHR surface;
        vk::UniqueHandle<vk::CommandPool, vk::DispatchLoaderDynamic> command_pool;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> render_commandbuffers;
        static constexpr uint32_t shadowmap_cascades {4};

        struct ShadowmapCascade
        {
            vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic> shadowmap_frame_buffer;
            vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> shadowmap_image_view;
        };

        std::array<ShadowmapCascade, shadowmap_cascades> cascades;

        vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> shadowmap_sampler;
        std::unique_ptr<Image> shadowmap_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> shadowmap_image_view;

        struct FramebufferAttachment
        {
            std::unique_ptr<Image> image;
            vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> image_view;
            vk::Format format;
        };

        struct GBuffer
        {
            FramebufferAttachment position;
            FramebufferAttachment normal;
            FramebufferAttachment albedo;
            FramebufferAttachment depth;

            vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic> frame_buffer;
            vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> sampler;
        } gbuffer;

        vk::UniqueHandle<vk::Semaphore, vk::DispatchLoaderDynamic> gbuffer_sem;

        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> deferred_command_buffers;

        vk::DispatchLoaderDynamic dispatch;

        RenderMode render_mode{ RenderMode::RTX };

        /* Animation pipeline */
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> animation_descriptor_set_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> animation_pipeline_layout;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> animation_pipeline;
        /* Animation pipeline */

        std::unique_ptr<Raytracer> raytracer;

        /* Ray tracing */
        bool RTXEnabled();
        bool RasterizationEnabled();
        vk::PhysicalDeviceRayTracingPropertiesNV ray_tracing_properties;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> rtx_descriptor_layout_const;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> rtx_descriptor_layout_dynamic;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> rtx_pipeline_layout;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> rtx_pipeline;
        vk::UniqueHandle<vk::DescriptorPool, vk::DispatchLoaderDynamic> rtx_descriptor_pool_const;
        vk::UniqueHandle<vk::DescriptorPool, vk::DispatchLoaderDynamic> rtx_descriptor_pool_dynamic;
        std::vector<vk::UniqueHandle<vk::DescriptorSet, vk::DispatchLoaderDynamic>> rtx_descriptor_sets_const;
        std::vector<vk::UniqueHandle<vk::DescriptorSet, vk::DispatchLoaderDynamic>> rtx_descriptor_sets_dynamic;
        std::vector<std::unique_ptr<Image>> rtx_render_targets;
        std::vector<vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic>> rtx_render_target_views;
        vk::DeviceSize shader_stride;
        struct shader_binding
        {
            uint32_t geometry_instance;
        };
        std::unique_ptr<Buffer> shader_binding_table;
        uint16_t static_acceleration_bindings_offset {0};
        std::mutex acceleration_binding_mutex;
        static constexpr uint16_t max_acceleration_binding_index{ 1024 };

    private:
        void createRayTracingResources();
        /* Ray tracing */

    private:
        void createInstance(const std::string& app_name, uint32_t app_version);
        void createPhysicalDevice();
        void createDevice();
        void createSwapchain();
        void createRenderpasses();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void createDepthImage();
        void createFramebuffers();
        void createSyncs();
        void createCommandPool();
        void createShadowmapResources();
        void createGBufferResources();
        void createDeferredCommandBuffer();
        void createQuad();
        void createAnimationResources();

        void recreateRenderer();
        void recreateStaticCommandBuffers();

        bool checkValidationLayerSupport() const;
        std::vector<const char*> getRequiredExtensions() const;
        vk::Format getDepthFormat() const;
        bool extensionsSupported(vk::PhysicalDevice device);

        struct swapChainInfo
        {
            vk::SurfaceCapabilitiesKHR capabilities;
            std::vector<vk::SurfaceFormatKHR> formats;
            std::vector<vk::PresentModeKHR> present_modes;
        };

        swapChainInfo getSwapChainInfo(vk::PhysicalDevice device) const;

        vk::CommandBuffer getRenderCommandbuffer(uint32_t image_index);

        Engine* engine;
        VkDebugUtilsMessengerEXT debug_messenger {nullptr};
        std::vector<vk::UniqueHandle<vk::Fence, vk::DispatchLoaderDynamic>> frame_fences;
        std::vector<vk::UniqueHandle<vk::Semaphore, vk::DispatchLoaderDynamic>> image_ready_sem;
        std::vector<vk::UniqueHandle<vk::Semaphore, vk::DispatchLoaderDynamic>> frame_finish_sem;
        vk::UniqueHandle<vk::Semaphore, vk::DispatchLoaderDynamic> compute_sem;
        uint32_t current_image{ 0 };
        uint32_t max_pending_frames{ 2 };
        uint32_t current_frame{ 0 };

        struct
        {
            std::unique_ptr<Buffer> vertex_buffer;
            std::unique_ptr<Buffer> index_buffer;
            uint32_t index_count;
        } quad;

        bool resize{ false };
    };
}
