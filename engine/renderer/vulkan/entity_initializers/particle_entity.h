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

        virtual void initEntity(RendererRaytrace*, Engine*) override;
        virtual void drawEntity(RendererRaytrace*, Engine*) override;

        virtual void initEntity(RendererRasterization*, Engine*) override;
        virtual void drawEntity(RendererRasterization*, Engine*) override;

        virtual void initEntity(RendererHybrid*, Engine*) override;
        virtual void drawEntity(RendererHybrid*, Engine*) override;
    private:
        void createBuffers(Renderer*, Engine*);
        void drawModel(Engine* engine, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout, size_t image);
        void drawMesh(Engine* engine, vk::CommandBuffer buffer, const Mesh& mesh, vk::PipelineLayout, uint32_t mesh_index);

        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueCommandBuffer command_buffer;
    };

}
