#include "particle_entity_init.h"
#include "engine/worker_thread.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/particle.h"

namespace lotus
{
    ParticleEntityInitTask::ParticleEntityInitTask(const std::shared_ptr<Particle>& _entity) : WorkItem(), entity(_entity)
    {
    }

    void ParticleEntityInitTask::Process(WorkerThread* thread)
    {
        entity->uniform_buffer = thread->engine->renderer.memory_manager->GetBuffer(sizeof(RenderableEntity::UniformBufferObject) * thread->engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    ParticleEntityReInitTask::ParticleEntityReInitTask(const std::shared_ptr<Particle>& entity) : ParticleEntityInitTask(entity)
    {
    }

    void ParticleEntityReInitTask::Process(WorkerThread* thread)
    {
    }
}
