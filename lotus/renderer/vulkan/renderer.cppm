module;

#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan_hpp_macros.hpp>

export module lotus:renderer.vulkan.renderer;

import :entity.component.camera;
import :renderer.memory;
import :renderer.model;
import :renderer.raytrace_query;
import :renderer.vulkan.common.async_compute;
import :renderer.vulkan.common.global_descriptors;
import :renderer.vulkan.pipelines.post_process;
import :renderer.vulkan.pipelines.raster;
import :renderer.vulkan.pipelines.raytrace;
import :renderer.vulkan.gpu;
import :renderer.vulkan.swapchain;
import :renderer.vulkan.ui_renderer;
import :renderer.vulkan.window;
import :util;
import glm;
import vulkan_hpp;

namespace lotus
{
export class Engine;
class TopLevelAccelerationStructure;
class BottomLevelAccelerationStructure;

class RendererRaytrace;
class RendererRasterization;
class RendererHybrid;

export class Renderer
{
public:
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
    void createDeferredImage();

    vk::CommandBuffer prepareDeferredImageForPresent();

    uint32_t getImageCount() const { return static_cast<uint32_t>(swapchain->images.size()); }
    uint32_t getCurrentImage() const { return current_image; }

    static constexpr uint32_t getFrameCount() { return max_pending_frames; }
    uint32_t getCurrentFrame() const { return current_frame; }
    uint32_t getPreviousFrame() const { return previous_frame; }
    void setCurrentFrame(int _current_frame)
    {
        previous_frame = current_frame;
        current_frame = _current_frame;
    }

    size_t uniform_buffer_align_up(size_t in_size) const;
    size_t storage_buffer_align_up(size_t in_size) const;
    static inline size_t align_up(size_t in_size, size_t alignment)
    {
        if (in_size % alignment == 0)
            return in_size;
        else
            return ((in_size / alignment) + 1) * alignment;
    }

    vk::Extent2D getExtent() const;

    virtual Task<> drawFrame() = 0;

    void resized() { resize = true; }

    vk::UniqueShaderModule getShader(const std::string& file_name);
    virtual vk::Pipeline createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info) = 0;
    virtual vk::Pipeline createParticlePipeline(vk::GraphicsPipelineCreateInfo& info) = 0;
    virtual vk::Pipeline createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info) = 0;

    vk::UniqueInstance instance;

    std::unique_ptr<Window> window;
    vk::UniqueSurfaceKHR surface;
    std::unique_ptr<GPU> gpu;
    std::unique_ptr<Swapchain> swapchain;
    std::unique_ptr<AsyncCompute> async_compute;

    std::unique_ptr<GlobalDescriptors> global_descriptors;

    inline static thread_local vk::UniqueCommandPool graphics_pool;
    inline static thread_local vk::UniqueCommandPool compute_pool;

    inline static thread_local vk::UniqueDescriptorPool desc_pool;

    std::unique_ptr<Image> deferred_image;
    vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic> deferred_image_view;

    struct
    {
        std::unique_ptr<Buffer> view_proj_ubo;
        Component::CameraComponent::CameraData* view_proj_mapped{nullptr};
        std::unique_ptr<Buffer> cascade_data_ubo;
        uint8_t* cascade_data_mapped{nullptr};
    } camera_buffers;

    // TODO: cascade stuff goes into shadowmap pipeline class
    static constexpr uint32_t shadowmap_cascades{4};

    struct UBOFS
    {
        glm::vec4 cascade_splits;
        glm::mat4 cascade_view_proj[shadowmap_cascades];
        glm::mat4 inverse_view;
    } cascade_data{};

protected:
    // when threads terminate, they move their thread_locals here so they can be destructed in the right order
    void deleteThreadLocals();
    std::mutex shutdown_mutex;
    std::vector<vk::UniqueCommandPool> shutdown_command_pools;
    std::vector<vk::UniqueDescriptorPool> shutdown_descriptor_pools;
    std::vector<vk::UniquePipeline> pipelines;

    vk::UniqueCommandPool command_pool;
    std::vector<vk::UniqueCommandBuffer> present_buffers;

public:
    struct FramebufferAttachment
    {
        std::unique_ptr<Image> image;
        vk::UniqueImageView image_view;
    };

    std::vector<vk::UniqueFramebuffer> frame_buffers;

    /* Animation pipeline */
    vk::UniqueDescriptorSetLayout animation_descriptor_set_layout;
    vk::UniquePipelineLayout animation_pipeline_layout;
    vk::UniquePipeline animation_pipeline;
    /* Animation pipeline */

    std::unique_ptr<RaytraceQueryer> raytrace_queryer;

    friend class RaytracePipeline;
    std::unique_ptr<RaytracePipeline> raytracer;
    std::unique_ptr<RasterPipeline> rasterizer;
    // TODO
    std::unique_ptr<RasterPipeline> shadowmap_rasterizer;
    friend class PostProcessPipeline;
    std::unique_ptr<PostProcessPipeline> post_process;
    std::unique_ptr<UiRenderer> ui;

protected:
    void createInstance(const std::string& app_name, uint32_t app_version);
    void createSwapchain();
    void createSemaphores();
    void createCommandPool();
    void createAnimationResources();

    Task<> resizeRenderer();
    virtual Task<> recreateRenderer() = 0;
    Task<> recreateStaticCommandBuffers();

    bool checkValidationLayerSupport() const;
    std::vector<const char*> getRequiredExtensions() const;

    Engine* engine;
    vk::UniqueDebugUtilsMessengerEXT debug_messenger;
    std::vector<vk::UniqueSemaphore> image_ready_sem;
    std::vector<vk::UniqueSemaphore> frame_finish_sem;
    std::vector<vk::UniqueSemaphore> frame_timeline_sem;
    std::vector<uint64_t> timeline_sem_base{};
    uint32_t current_image{0};
    uint32_t previous_image{0};
    static constexpr uint32_t max_pending_frames{2};
    uint32_t current_frame{0};
    uint32_t previous_frame{0};

    bool resize{false};
};
} // namespace lotus
