#pragma once
#include "core/work_item.h"
#include "core/entity/landscape_entity.h"

namespace lotus
{
    class LandscapeEntityInitTask : public WorkItem
    {
    public:
        LandscapeEntityInitTask(const std::shared_ptr<LandscapeEntity>& entity, std::vector<LandscapeEntity::InstanceInfo>&& instance_info);
        virtual void Process(WorkerThread*) override;
    private:
        void drawModel(WorkerThread* thread, vk::CommandBuffer buffer, vk::Buffer uniform_buffer, bool transparency);
        void drawMesh(WorkerThread* thread, vk::CommandBuffer buffer, const Mesh& mesh, vk::Buffer uniform_buffer, vk::DeviceSize count);
        void drawBlendedMeshes(WorkerThread* thread, vk::DeviceSize index, vk::Buffer uniform_buffer);
        void drawBlendedMesh(WorkerThread* thread, vk::CommandBuffer buffer, Mesh* mesh, vk::Buffer uniform_buffer);
        void populateInstanceBuffer(WorkerThread* thread);
        std::shared_ptr<LandscapeEntity> entity;
        std::vector<LandscapeEntity::InstanceInfo> instance_info;
        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };
}
