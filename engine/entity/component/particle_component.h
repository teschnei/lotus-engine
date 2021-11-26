#pragma once
#include "component.h"
#include <memory>
#include <vector>
#include "engine/worker_task.h"
#include "engine/renderer/model.h"

namespace lotus::Component
{
    class ParticleComponent : public Component<ParticleComponent>
    {
    public:

        explicit ParticleComponent(Entity*, Engine* engine, std::vector<std::shared_ptr<Model>> models);

        Task<> tick(time_point time, duration delta);

        std::vector<std::shared_ptr<Model>> getModels() const;

        glm::vec4 color{ 1.f };
        glm::vec2 uv_offset{ 0.f };
        uint64_t resource_index{ 0 };
        uint16_t current_sprite{ 0 };
        uint32_t pipeline_index{ 0 };
        time_point start_time{};

    protected:
        std::vector<std::shared_ptr<Model>> models;
    };
}
