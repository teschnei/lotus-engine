#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <glm/glm.hpp>
#include "window.h"
#include "gpu.h"
#include "swapchain.h"
#include "ui_renderer.h"
#include "engine/renderer/raytrace_query.h"
#include "engine/task.h"

namespace lotus
{
    class Engine;
    class Entity;
    class RenderableEntity;
    class WorkerThread;
    class TopLevelAccelerationStructure;
    class BottomLevelAccelerationStructure;

    class RendererRaytrace;
    class RendererRasterization;
    class RendererHybrid;

    class EntityInitializer
    {
    public:
        EntityInitializer(Entity* _entity) : entity(_entity) {}
        virtual ~EntityInitializer() {}

        virtual void initEntity(RendererRaytrace* renderer, Engine* engine) = 0;
        virtual void drawEntity(RendererRaytrace* renderer, Engine* engine) = 0;

        virtual void initEntity(RendererRasterization* renderer, Engine* engine) = 0;
        virtual void drawEntity(RendererRasterization* renderer, Engine* engine) = 0;

        virtual void initEntity(RendererHybrid* renderer, Engine* engine) = 0;
        virtual void drawEntity(RendererHybrid* renderer, Engine* engine) = 0;
    protected:
        Entity* entity;
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
            std::vector<vk::VertexInputBindingDescription> particle_vertex_input_binding_descriptions;
            std::vector<vk::VertexInputAttributeDescription> particle_vertex_input_attribute_descriptions;
        };
        Renderer(Engine* engine);
        virtual ~Renderer();

        Task<> InitCommon();
        virtual Task<> Init() = 0;

        struct ThreadLocals
        {
            ThreadLocals(Renderer* _renderer) : renderer(_renderer) {}
            ~ThreadLocals() { renderer->deleteThreadLocals(); }
            ThreadLocals(const ThreadLocals&) = delete;
            ThreadLocals(ThreadLocals&&) = delete;
            ThreadLocals& operator=(const ThreadLocals&) = delete;
            ThreadLocals& operator=(ThreadLocals&&) = delete;
        private:
            Renderer* renderer;
        };
        [[nodiscard]]
        ThreadLocals createThreadLocals();

        uint32_t getImageCount() const { return static_cast<uint32_t>(swapchain->images.size()); }
        uint32_t getCurrentImage() const { return current_image; }
        void setCurrentImage(int _current_image) { current_image = _current_image; }
        size_t uniform_buffer_align_up(size_t in_size) const;
        size_t storage_buffer_align_up(size_t in_size) const;
        size_t align_up(size_t in_size, size_t alignment) const;

        virtual Task<> drawFrame() = 0;
        virtual void populateAccelerationStructure(TopLevelAccelerationStructure*, BottomLevelAccelerationStructure*, const glm::mat3x4&, uint64_t, uint32_t, uint32_t) = 0;

        virtual void initEntity(EntityInitializer*, Engine*) = 0;
        virtual void drawEntity(EntityInitializer*, Engine*) = 0;

        void resized() { resize = true; }

        vk::UniqueHandle<vk::ShaderModule, vk::DispatchLoaderDynamic> getShader(const std::string& file_name);

        vk::UniqueHandle<vk::Instance, vk::DispatchLoaderDynamic> instance;

        std::unique_ptr<Window> window;
        vk::UniqueSurfaceKHR surface;
        std::unique_ptr<GPU> gpu;
        std::unique_ptr<Swapchain> swapchain;

        vk::UniqueHandle<vk::CommandPool, vk::DispatchLoaderDynamic> command_pool;

        inline static thread_local vk::UniqueCommandPool graphics_pool;
        inline static thread_local vk::UniqueCommandPool compute_pool;

        inline static thread_local vk::UniqueDescriptorPool desc_pool;

    private:
        //when threads terminate, they move their thread_locals here so they can be destructed in the right order
        void deleteThreadLocals();
        std::mutex shutdown_mutex;
        std::vector<vk::UniqueCommandPool> shutdown_command_pools;
        std::vector<vk::UniqueDescriptorPool> shutdown_descriptor_pools;
    public:

        struct FramebufferAttachment
        {
            std::unique_ptr<Image> image;
            vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> image_view;
        };

        std::vector<vk::UniqueHandle<vk::Framebuffer, vk::DispatchLoaderDynamic>> frame_buffers;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> deferred_command_buffers;

        /* Animation pipeline */
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> animation_descriptor_set_layout;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> animation_pipeline_layout;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> animation_pipeline;
        /* Animation pipeline */

        std::unique_ptr<Raytracer> raytracer;
        std::unique_ptr<UiRenderer> ui;

    protected:
        void createInstance(const std::string& app_name, uint32_t app_version);
        void createSwapchain();
        void createCommandPool();
        void createQuad();
        void createAnimationResources();

        Task<> resizeRenderer();
        Task<> recreateRenderer();
        Task<> recreateStaticCommandBuffers();


        bool checkValidationLayerSupport() const;
        std::vector<const char*> getRequiredExtensions() const;

        virtual vk::CommandBuffer getRenderCommandbuffer(uint32_t image_index) = 0;

        Engine* engine;
        vk::UniqueDebugUtilsMessengerEXT debug_messenger;
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
