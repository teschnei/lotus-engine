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
        ParticleEntityInitializer(Entity* _entity, ParticleEntityInitTask* task);

        virtual void initEntity(RendererRaytrace*, WorkerThread*) override;
        virtual void drawEntity(RendererRaytrace*, WorkerThread*) override;

        virtual void initEntity(RendererRasterization*, WorkerThread*) override;
        virtual void drawEntity(RendererRasterization*, WorkerThread*) override;

        virtual void initEntity(RendererHybrid*, WorkerThread*) override;
        virtual void drawEntity(RendererHybrid*, WorkerThread*) override;
    private:
        void createBuffers(Renderer*, WorkerThread*);
        void drawModel(WorkerThread* thread, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout, size_t image);
        void drawMesh(WorkerThread* thread, vk::CommandBuffer buffer, const Mesh& mesh, vk::PipelineLayout, uint32_t mesh_index);

        ParticleEntityInitTask* task;
        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueCommandBuffer command_buffer;
    };

}