#include "physics_component.h"
#include "engine/core.h"

namespace lotus::Component
{
    PhysicsComponent::PhysicsComponent(Entity* _entity, Engine* _engine) :
         Component(_entity, _engine)
    {
    }

    Task<> PhysicsComponent::init()
    {
        uniform_buffer = engine->renderer->gpu->memory_manager->GetBuffer(engine->renderer->uniform_buffer_align_up(sizeof(UniformBufferObject)) * engine->renderer->getFrameCount(),
            vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uniform_buffer_mapped = static_cast<UniformBufferObject*>(uniform_buffer->map(0, engine->renderer->uniform_buffer_align_up(sizeof(UniformBufferObject)) * engine->renderer->getFrameCount(), {}));
        co_return;
    }

    Task<> PhysicsComponent::tick(time_point time, duration delta)
    {
        model_prev = model;
        if (should_update_matrix)
        {
            model = glm::translate(glm::mat4{ 1.f }, pos) * glm::transpose(glm::toMat4(rot)) * glm::scale(glm::mat4{ 1.f }, scale);
            modelT = glm::transpose(model);
            modelIT = glm::transpose(glm::inverse(glm::mat3(model)));
            should_update_matrix = false;
        }
        uniform_buffer_mapped[engine->renderer->getCurrentFrame()].model = model;
        uniform_buffer_mapped[engine->renderer->getCurrentFrame()].modelIT = modelIT;
        uniform_buffer_mapped[engine->renderer->getCurrentFrame()].model_prev = model_prev;
        co_return;
    }

    std::tuple<vk::Buffer, size_t, size_t> PhysicsComponent::getUniformBuffer(uint32_t image_index) const
    {
        return { uniform_buffer->buffer, image_index * engine->renderer->uniform_buffer_align_up(sizeof(UniformBufferObject)), sizeof(UniformBufferObject) };
    }

    void PhysicsComponent::setPos(glm::vec3 _pos)
    {
        pos = _pos;
        should_update_matrix = true;
    }

    void PhysicsComponent::setRot(glm::quat _rot)
    {
        rot = _rot;
        should_update_matrix = true;
    }

    void PhysicsComponent::setScale(glm::vec3 _scale)
    {
        scale = _scale;
        should_update_matrix = true;
    }
}
