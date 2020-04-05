#pragma once
#include "../work_item.h"
#include "../entity/renderable_entity.h"
#include <engine/renderer/vulkan/vulkan_inc.h>

namespace lotus
{
    class Particle;
    class ParticleEntityInitTask : public WorkItem
    {
    public:
        ParticleEntityInitTask(const std::shared_ptr<Particle>& entity);
        virtual void Process(WorkerThread*) override;
    protected:
        std::shared_ptr<Particle> entity;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };


    class ParticleEntityReInitTask : public ParticleEntityInitTask
    {
    public:
        ParticleEntityReInitTask(const std::shared_ptr<Particle>& entity);
        virtual void Process(WorkerThread*) override;
    };

}
