module;

#include <chrono>
#include <coroutine>
#include <memory>
#include <vector>

export module lotus:entity.component.particle;

import :core.engine;
import :entity.component;
import :renderer.model;
import :renderer.vulkan.renderer;
import :util;
import glm;

export namespace lotus::Component
{
class ParticleComponent : public Component<ParticleComponent>
{
public:
    explicit ParticleComponent(Entity*, Engine* engine, std::shared_ptr<Model> models);

    Task<> tick(time_point time, duration delta);

    std::pair<std::shared_ptr<Model>, GlobalDescriptors::MeshInfoBuffer::View*> getModel() const;

    glm::vec4 color{1.f};
    glm::vec2 uv_offset{0.f};
    uint16_t current_sprite{0};
    uint32_t pipeline_index{0};
    time_point start_time{};

protected:
    std::shared_ptr<Model> model;
    std::vector<std::unique_ptr<GlobalDescriptors::MeshInfoBuffer::View>> mesh_infos;
};

ParticleComponent::ParticleComponent(Entity* _entity, Engine* _engine, std::shared_ptr<Model> _model)
    : Component(_entity, _engine), model(_model), start_time(engine->getSimulationTime())
{
}

Task<> ParticleComponent::tick(time_point time, duration delta)
{
    if (model->meshes[0]->getSpriteCount() > 1)
    {
        auto diff = time - start_time;
        auto frames = 60.f * ((float)std::chrono::nanoseconds(diff).count() / std::chrono::nanoseconds(1s).count());
        current_sprite = static_cast<size_t>(frames) % model->meshes[0]->getSpriteCount();
    }

    mesh_infos[engine->renderer->getCurrentFrame()]->buffer_view[0].colour = color;
    mesh_infos[engine->renderer->getCurrentFrame()]->buffer_view[0].uv_offset = uv_offset;
    mesh_infos[engine->renderer->getCurrentFrame()]->buffer_view[0].animation_frame = model->animation_frame;

    co_return;
}

std::pair<std::shared_ptr<Model>, GlobalDescriptors::MeshInfoBuffer::View*> ParticleComponent::getModel() const
{
    return {model, mesh_infos[engine->renderer->getCurrentFrame()].get()};
}
} // namespace lotus::Component
