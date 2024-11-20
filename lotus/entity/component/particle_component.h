#pragma once
#include "component.h"
#include <memory>
#include <vector>
#include "lotus/worker_task.h"
#include "lotus/renderer/model.h"

namespace lotus::Component
{
    class ParticleComponent : public Component<ParticleComponent>
    {
    public:

        explicit ParticleComponent(Entity*, Engine* engine, std::shared_ptr<Model> models);

        Task<> tick(time_point time, duration delta);

        std::pair<std::shared_ptr<Model>, GlobalDescriptors::MeshInfoBuffer::View*> getModel() const;

        glm::vec4 color{ 1.f };
        glm::vec2 uv_offset{ 0.f };
        uint16_t current_sprite{ 0 };
        uint32_t pipeline_index{ 0 };
        time_point start_time{};

    protected:
        std::shared_ptr<Model> model;
        std::vector<std::unique_ptr<GlobalDescriptors::MeshInfoBuffer::View>> mesh_infos;
    };
}
