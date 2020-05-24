#pragma once

#include <memory>
#include "renderable_entity.h"

namespace lotus
{
    class Particle : public RenderableEntity
    {
    public:
        Particle(Engine*);
        void Init(const std::shared_ptr<Particle>& sp, duration lifetime);
        virtual ~Particle() = default;

        bool billboard{ false };
        glm::vec4 color{ 1.f };

        duration getLifetime() { return lifetime; }
        time_point getSpawnTime() { return spawn_time; }

        virtual void populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index);

        uint64_t resource_index{ 0 };

    protected:
        virtual void tick(time_point time, duration delta) override;
        virtual void render(Engine* engine, std::shared_ptr<Entity>& sp) override;
        glm::mat4 entity_rot_mat{};

        duration lifetime;
        time_point spawn_time;
    };
}
