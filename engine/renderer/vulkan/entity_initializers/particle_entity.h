#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/renderer/vulkan/vulkan_inc.h"

namespace lotus
{
    class RendererRaytrace;
    class RendererRasterization;
    class RendererHybrid;
    class WorkerThread;
    class ParticleEntityInitTask;
    class Mesh;

    class ParticleEntityInitializer : public EntityInitializer
    {
    public:
        ParticleEntityInitializer(Entity* _entity);

        virtual void initEntity(RendererRaytrace* renderer, Engine* engine) override;

        virtual void initEntity(RendererRasterization* renderer, Engine* engine) override;
        virtual void drawEntity(RendererRasterization* renderer, Engine* engine, vk::CommandBuffer buffer, uint32_t image) override;
        virtual void drawEntityShadowmap(RendererRasterization* renderer, Engine* engine, vk::CommandBuffer buffer, uint32_t image) {}

        virtual void initEntity(RendererHybrid* renderer, Engine* engine) override;
        virtual void drawEntity(RendererHybrid* renderer, Engine* engine, vk::CommandBuffer buffer, uint32_t image) override;
    private:
        void createBuffers(Renderer*, Engine*);
        void drawModel(Engine* engine, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout, size_t image);
        void drawMesh(Engine* engine, vk::CommandBuffer buffer, const Mesh& mesh, vk::PipelineLayout, uint32_t mesh_index);

        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueCommandBuffer command_buffer;
    };

}
