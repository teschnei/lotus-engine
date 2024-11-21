#pragma once

#include "lotus/renderer/vulkan/renderer.h"

namespace lotus
{
class Engine;

class RendererRaytrace : public Renderer
{
public:
    RendererRaytrace(Engine* engine);
    ~RendererRaytrace();

    virtual Task<> Init() override;
    WorkerTask<> InitWork();

    virtual Task<> drawFrame() override;

    std::unique_ptr<Image> depth_image;
    vk::UniqueImageView depth_image_view;
    std::vector<vk::UniqueCommandBuffer> render_commandbuffers;

    virtual vk::Pipeline createGraphicsPipeline(vk::GraphicsPipelineCreateInfo& info) override { return {}; }
    virtual vk::Pipeline createParticlePipeline(vk::GraphicsPipelineCreateInfo& info) override { return {}; }
    virtual vk::Pipeline createShadowmapPipeline(vk::GraphicsPipelineCreateInfo& info) override { return {}; }

    /* Ray tracing */
    vk::UniqueDescriptorSetLayout descriptor_layout_deferred;
    vk::UniquePipelineLayout deferred_pipeline_layout;
    vk::UniquePipeline deferred_pipeline;

    struct RaytraceGBuffer
    {
        FramebufferAttachment albedo;
        FramebufferAttachment light;
        FramebufferAttachment normal;
        FramebufferAttachment particle;
        FramebufferAttachment motion_vector;

        vk::UniqueSampler sampler;
    } gbuffer;

private:
    void createDescriptorSetLayout();
    void createRaytracingPipeline();
    void createGraphicsPipeline();
    void createDepthImage();
    void createSyncs();
    void createGBufferResources();

    virtual Task<> recreateRenderer() override;

    void initializeCameraBuffers();

    vk::CommandBuffer getRenderCommandbuffer();
    vk::UniqueCommandBuffer getDeferredCommandBuffer();
};
} // namespace lotus
