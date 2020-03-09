#pragma once
#include "../work_item.h"
#include "../entity/renderable_entity.h"
#include <engine/renderer/vulkan/vulkan_inc.h>

namespace lotus
{
    class RenderableEntityInitTask : public WorkItem
    {
    public:
        RenderableEntityInitTask(const std::shared_ptr<RenderableEntity>& entity);
        virtual void Process(WorkerThread*) override;
    protected:
        void createStaticCommandBuffers(WorkerThread* thread);
        void drawModel(WorkerThread* thread, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout, size_t image);
        void drawMesh(WorkerThread* thread, vk::CommandBuffer buffer, const Mesh& mesh, vk::PipelineLayout, uint32_t material_index);
        void generateVertexBuffers(WorkerThread* thread, vk::CommandBuffer buffer, const Model& mesh, std::vector<std::vector<std::unique_ptr<Buffer>>>& vertex_buffer);
        std::shared_ptr<RenderableEntity> entity;
        std::vector<std::unique_ptr<Buffer>> staging_buffers;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };


    class RenderableEntityReInitTask : public RenderableEntityInitTask
    {
    public:
        RenderableEntityReInitTask(const std::shared_ptr<RenderableEntity>& entity);
        virtual void Process(WorkerThread*) override;
    };

}
