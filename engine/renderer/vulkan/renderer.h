#pragma once

#include <string>
#include <glm/glm.hpp>
#include "window.h"
#include "gpu.h"
#include "swapchain.h"
#include "global_resources.h"
#include "ui_renderer.h"
#include "engine/renderer/raytrace_query.h"
#include "engine/task.h"
#include "engine/renderer/model.h"

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
    class RenderableEntityInitializer;

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
            uint32_t shadowmap_dimension{ 2048 };
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
        virtual void initModel(RenderableEntityInitializer*, Engine*, Model& model, ModelTransformedGeometry& model_transform) = 0;

        void resized() { resize = true; }

        vk::UniqueShaderModule getShader(const std::string& file_name);
        virtual vk::Pipeline createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info) = 0;
        virtual vk::Pipeline createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info) = 0;

        virtual void bindResources(uint32_t image, vk::WriteDescriptorSet vertex, vk::WriteDescriptorSet index,
            vk::WriteDescriptorSet material, vk::WriteDescriptorSet texture, vk::WriteDescriptorSet mesh_info) = 0;

        vk::UniqueInstance instance;

        std::unique_ptr<Window> window;
        vk::UniqueSurfaceKHR surface;
        std::unique_ptr<GPU> gpu;
        std::unique_ptr<Swapchain> swapchain;
        std::unique_ptr<GlobalResources> resources;

        inline static thread_local vk::UniqueCommandPool graphics_pool;
        inline static thread_local vk::UniqueCommandPool compute_pool;

        inline static thread_local vk::UniqueDescriptorPool desc_pool;

    protected:
        //when threads terminate, they move their thread_locals here so they can be destructed in the right order
        void deleteThreadLocals();
        std::mutex shutdown_mutex;
        std::vector<vk::UniqueCommandPool> shutdown_command_pools;
        std::vector<vk::UniqueDescriptorPool> shutdown_descriptor_pools;
        std::vector<vk::UniquePipeline> pipelines;

        vk::UniqueCommandPool command_pool;
        vk::UniqueCommandPool local_compute_pool;
    public:

        struct FramebufferAttachment
        {
            std::unique_ptr<Image> image;
            vk::UniqueImageView image_view;
        };

        std::vector<vk::UniqueFramebuffer> frame_buffers;
        std::vector<vk::UniqueCommandBuffer> deferred_command_buffers;

        /* Animation pipeline */
        vk::UniqueDescriptorSetLayout animation_descriptor_set_layout;
        vk::UniquePipelineLayout animation_pipeline_layout;
        vk::UniquePipeline animation_pipeline;
        /* Animation pipeline */

        std::unique_ptr<Raytracer> raytracer;
        std::unique_ptr<UiRenderer> ui;

        /* Post processing */
        vk::UniqueDescriptorSetLayout post_descriptor_set_layout;
        vk::UniquePipelineLayout post_pipeline_layout;
        vk::UniquePipeline post_pipeline;
        std::vector<vk::UniqueCommandBuffer> post_command_buffers;
        /* Post processing */

    protected:
        void createInstance(const std::string& app_name, uint32_t app_version);
        void createSwapchain();
        void createCommandPool();
        void createAnimationResources();

        Task<> resizeRenderer();
        virtual Task<> recreateRenderer() = 0;
        Task<> recreateStaticCommandBuffers();

        bool checkValidationLayerSupport() const;
        std::vector<const char*> getRequiredExtensions() const;

        virtual vk::CommandBuffer getRenderCommandbuffer(uint32_t image_index) = 0;

        Engine* engine;
        vk::UniqueDebugUtilsMessengerEXT debug_messenger;
        std::vector<vk::UniqueFence> frame_fences;
        std::vector<vk::UniqueSemaphore> image_ready_sem;
        std::vector<vk::UniqueSemaphore> frame_finish_sem;
        vk::UniqueSemaphore compute_sem;
        uint32_t current_image{ 0 };
        static constexpr uint32_t max_pending_frames{ 2 };
        uint32_t current_frame{ 0 };

        bool resize{ false };
    };
}
