#pragma once
#include "../work_item.h"
#include "../entity/renderable_entity.h"
#include <vulkan/vulkan.hpp>

namespace lotus
{
    class RenderableEntityInitTask : public WorkItem
    {
    public:
        RenderableEntityInitTask(const std::shared_ptr<RenderableEntity>& entity);
        virtual void Process(WorkerThread*) override;
    private:
        void drawModel(WorkerThread* thread, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout);
        void drawMesh(WorkerThread* thread, vk::CommandBuffer buffer, const Mesh& mesh, vk::PipelineLayout);
        std::shared_ptr<RenderableEntity> entity;
    };
}
