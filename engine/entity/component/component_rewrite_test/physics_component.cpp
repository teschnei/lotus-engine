#include "physics_component.h"
#include "engine/core.h"

namespace lotus::Test
{
    PhysicsComponent::PhysicsComponent(Entity* _entity, Engine* _engine) :
         Component(_entity, _engine)
    {
        //pos = (glm::vec3(259.f, -87.f, 99.f));
        pos = (glm::vec3(-681.f, -12.f, 161.f));
    }

    Task<> PhysicsComponent::init()
    {
        uniform_buffer = engine->renderer->gpu->memory_manager->GetBuffer(engine->renderer->uniform_buffer_align_up(sizeof(UniformBufferObject)) * engine->renderer->getImageCount(),
            vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uniform_buffer_mapped = static_cast<UniformBufferObject*>(uniform_buffer->map(0, engine->renderer->uniform_buffer_align_up(sizeof(UniformBufferObject)) * engine->renderer->getImageCount(), {}));
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
        uniform_buffer_mapped[engine->renderer->getCurrentImage()].model = model;
        uniform_buffer_mapped[engine->renderer->getCurrentImage()].modelIT = modelIT;
        uniform_buffer_mapped[engine->renderer->getCurrentImage()].model_prev = model_prev;
        co_return;
    }

    std::tuple<vk::Buffer, size_t, size_t> PhysicsComponent::getUniformBuffer(uint32_t image_index) const
    {
        return { uniform_buffer->buffer, image_index * engine->renderer->uniform_buffer_align_up(sizeof(UniformBufferObject)), sizeof(UniformBufferObject) };
    }
}
