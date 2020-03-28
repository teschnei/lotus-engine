#pragma once

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

    protected:
        virtual void tick(time_point time, duration delta) override;

        duration lifetime;
        time_point spawn_time;
    };
}
