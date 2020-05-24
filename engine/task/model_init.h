#pragma once

#include "../work_item.h"
#include "../renderer/mesh.h"
#include "../renderer/model.h"

namespace lotus
{
    class ModelInitTask : public WorkItem
    {
    public:
        ModelInitTask(int image_index, std::shared_ptr<Model> model, std::vector<std::vector<uint8_t>>&& vertex_buffers, std::vector<std::vector<uint8_t>>&& index_buffers, uint32_t vertex_stride);
        virtual ~ModelInitTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        int image_index;
        std::shared_ptr<Model> model;
        std::vector<std::vector<uint8_t>> vertex_buffers;
        std::vector<std::vector<uint8_t>> index_buffers;
        uint32_t vertex_stride;
        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };
}
