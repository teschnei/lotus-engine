#pragma once

#include "engine/work_item.h"
#include "engine/entity/particle.h"

namespace lotus
{
    class ParticleRenderTask : public WorkItem
    {
    public:
        ParticleRenderTask(std::shared_ptr<Particle>& particle, float priority = 1);

        virtual void Process(WorkerThread*) override;
    private:
        void drawModel(WorkerThread* thread, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout, size_t image);
        void drawMesh(WorkerThread* thread, vk::CommandBuffer buffer, const Mesh& mesh, vk::PipelineLayout, uint32_t material_index);

        std::shared_ptr<Particle> particle;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };
}
