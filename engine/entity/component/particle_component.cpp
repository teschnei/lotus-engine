#include "particle_component.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"

namespace lotus::Component
{
    ParticleComponent::ParticleComponent(Entity* _entity, Engine* _engine, std::vector<std::shared_ptr<Model>> _models) :
        Component(_entity, _engine), models(_models), start_time(engine->getSimulationTime())
    {
    }

    Task<> ParticleComponent::tick(time_point time, duration delta)
    {
        if (models[0]->meshes[0]->getSpriteCount() > 1)
        {
            auto diff = time - start_time;
            auto frames = 60.f * ((float)std::chrono::nanoseconds(diff).count() / std::chrono::nanoseconds(1s).count());
            current_sprite = static_cast<size_t>(frames) % models[0]->meshes[0]->getSpriteCount();
        }

        std::vector<GlobalResources::MeshInfo> mesh_info;
        for (size_t j = 0; j < models[0]->meshes.size(); ++j)
        {
            const auto& mesh = models[0]->meshes[j];
            mesh_info.push_back({
                .vertex_offset = 0,
                .index_offset = 0,
                .indices = (uint32_t)mesh->getIndexCount(),
                .material_index = 0,
                .scale = glm::vec3(1.f),//base_component.getScale(),
                .billboard = 0,//base_component.getBillboard(),
                .colour = color,
                .uv_offset = uv_offset,
                .animation_frame = models[0]->animation_frame,
                .vertex_prev_offset = 0,
                .model_prev = glm::mat4(1.f),//base_component.getPrevModelMatrix()
            });
        }

        resource_index = engine->renderer->resources->pushMeshInfo(mesh_info);

        co_return;
    }

    std::vector<std::shared_ptr<Model>> ParticleComponent::getModels() const
    {
        return models;
    }
}
