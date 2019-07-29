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
        void drawModel(WorkerThread* thread, vk::CommandBuffer buffer, Model& model, Texture& texture, vk::Buffer uniform_buffer);
        void drawPiece(WorkerThread* thread, vk::CommandBuffer buffer, Model& model, Texture& texture, vk::Buffer uniform_buffer);
        void createBuffers(WorkerThread* thread, RenderableEntity* entity);
        std::shared_ptr<RenderableEntity> entity;
    };
}
