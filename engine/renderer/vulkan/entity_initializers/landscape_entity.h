#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/renderer/vulkan/vulkan_inc.h"
#include "engine/entity/landscape_entity.h"

namespace lotus
{
    class RendererRaytrace;
    class RendererRasterization;
    class RendererHybrid;
    class WorkerThread;
    class LandscapeEntityInitTask;

    class LandscapeEntityInitializer : public EntityInitializer
    {
    public:
        LandscapeEntityInitializer(Entity* _entity, std::vector<LandscapeEntity::InstanceInfo>&& instance_info, LandscapeEntityInitTask* task);

        virtual void initEntity(RendererRaytrace*, WorkerThread*) override;
        virtual void drawEntity(RendererRaytrace*, WorkerThread*) override;

        virtual void initEntity(RendererRasterization*, WorkerThread*) override;
        virtual void drawEntity(RendererRasterization*, WorkerThread*) override;

        virtual void initEntity(RendererHybrid*, WorkerThread*) override;
        virtual void drawEntity(RendererHybrid*, WorkerThread*) override;
    private:
        void createBuffers(Renderer*, WorkerThread*);
        void drawModel(WorkerThread* thread, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout);
        void drawMesh(WorkerThread* thread, vk::CommandBuffer buffer, const Mesh& mesh, uint32_t count, vk::PipelineLayout, uint32_t material_index);

        std::vector<LandscapeEntity::InstanceInfo> instance_info;
        LandscapeEntityInitTask* task;
        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueCommandBuffer command_buffer;
    };

}