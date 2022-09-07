#include "particle_component.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus::Component
{
    ParticleComponent::ParticleComponent(Entity* _entity, Engine* _engine, std::shared_ptr<Model> _model) :
        Component(_entity, _engine), model(_model), start_time(engine->getSimulationTime())
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
        return { model, mesh_infos[engine->renderer->getCurrentFrame()].get() };
    }
}
