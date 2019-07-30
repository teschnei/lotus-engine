#pragma once

#include "../work_item.h"
#include "../renderer/mesh.h"

namespace lotus
{
    class ModelInitTask : public WorkItem
    {
    public:
        ModelInitTask(int image_index, std::shared_ptr<Mesh> model, std::vector<uint8_t>&& vertex_buffer, std::vector<uint8_t>&& index_buffer);
        virtual ~ModelInitTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        int image_index;
        std::shared_ptr<Mesh> model;
        std::vector<uint8_t> vertex_buffer;
        std::vector<uint8_t> index_buffer;
        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };
}
