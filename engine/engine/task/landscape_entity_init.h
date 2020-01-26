#pragma once
#include "engine/work_item.h"
#include "engine/entity/landscape_entity.h"

namespace lotus
{
    class LandscapeEntityInitTask : public WorkItem
    {
    public:
        LandscapeEntityInitTask(const std::shared_ptr<LandscapeEntity>& entity, std::vector<LandscapeEntity::InstanceInfo>&& instance_info);
        virtual void Process(WorkerThread*) override;
    protected:
        void createCommandBuffers(WorkerThread* thread);
        void drawModel(WorkerThread* thread, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout);
        void drawMesh(WorkerThread* thread, vk::CommandBuffer buffer, const Mesh& mesh, uint32_t count, vk::PipelineLayout);
        void populateInstanceBuffer(WorkerThread* thread);
        std::shared_ptr<LandscapeEntity> entity;
        std::vector<LandscapeEntity::InstanceInfo> instance_info;
        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };

    class LandscapeEntityReInitTask : public LandscapeEntityInitTask
    {
    public:
        LandscapeEntityReInitTask(const std::shared_ptr<LandscapeEntity>& entity);
        virtual void Process(WorkerThread*) override;
    };
}
