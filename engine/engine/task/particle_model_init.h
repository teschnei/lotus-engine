#pragma once

#include "engine/work_item.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/model.h"

namespace lotus
{
    class ParticleModelInitTask : public WorkItem
    {
    public:
        ParticleModelInitTask(int image_index, std::shared_ptr<Model> model, std::vector<uint8_t>&& vertex_buffer, uint32_t vertex_stride, float aabb_dist);
        virtual ~ParticleModelInitTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        int image_index;
        std::shared_ptr<Model> model;
        std::vector<uint8_t> vertex_buffer;
        uint32_t vertex_stride;
        float aabb_dist;
        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };
}
