#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <SDL.h>
#include "memory.h"

namespace lotus
{
    class Engine;

    class Renderer
    {
    public:
        Renderer(Engine* engine, const std::string& app_name, uint32_t app_version);
        ~Renderer();

        size_t getImageCount() const { return swapchain_images.size(); }
        uint32_t getCurrentImage() const { return current_image; }
        void setCurrentImage(int _current_image) { current_image = _current_image; }
        std::pair<std::optional<uint32_t>, std::optional<std::uint32_t>> getQueueFamilies(vk::PhysicalDevice device) const;

        void drawFrame();
        void resized() { resize = true; }

        vk::UniqueHandle<vk::Instance, vk::DispatchLoaderStatic> instance;
        vk::PhysicalDevice physical_device;
        vk::UniqueHandle<vk::Device, vk::DispatchLoaderStatic> device;
        vk::Queue graphics_queue;
        vk::Queue present_queue;
        vk::UniqueHandle<vk::SwapchainKHR, vk::DispatchLoaderDynamic> swapchain;
        vk::UniqueHandle<vk::SwapchainKHR, vk::DispatchLoaderDynamic> old_swapchain;
        vk::Extent2D swapchain_extent{};
        vk::Format swapchain_image_format{};
        std::vector<vk::Image> swapchain_images;
        std::vector<vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic>> swapchain_image_views;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> render_pass;
        vk::UniqueHandle<vk::RenderPass, vk::DispatchLoaderDynamic> shadowmap_render_pass;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> static_descriptor_set_layout;
        //vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> material_descriptor_set_layout;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> shadowmap_descriptor_set_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> pipeline_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> shadowmap_pipeline_layout;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> main_graphics_pipeline;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> blended_graphics_pipeline;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> shadowmap_pipeline;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> blended_shadowmap_pipeline;
        std::unique_ptr<Image> depth_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> depth_image_view;
        std::vector<vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic>> frame_buffers;
        SDL_Window* window {nullptr};
        vk::SurfaceKHR surface;
        std::unique_ptr<MemoryManager> memory_manager;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> render_commandbuffers;
        static constexpr size_t shadowmap_cascades {4};

        struct ShadowmapCascade
        {
            vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic> shadowmap_frame_buffer;
            vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> shadowmap_image_view;

            float split_depth;
            glm::mat4 view_proj_matrix;
        };

        std::array<ShadowmapCascade, shadowmap_cascades> cascades;

        vk::UniqueHandle<vk::Sampler, vk::DispatchLoaderDynamic> shadowmap_sampler;
        std::unique_ptr<Image> shadowmap_image;
        vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> shadowmap_image_view;
        vk::DispatchLoaderDynamic dispatch;
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

        vk::UniqueHandle<vk::ShaderModule, vk::DispatchLoaderDynamic> getShader(const std::string& file_name);

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
        vk::UniqueHandle<vk::CommandPool, vk::DispatchLoaderStatic> command_pool;
        uint32_t current_image{ 0 };
        uint32_t max_pending_frames{ 2 };
        uint32_t current_frame{ 0 };

        bool resize{ false };
    };
}
