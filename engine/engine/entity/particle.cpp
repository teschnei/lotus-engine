#include "particle.h"
#include "engine/core.h"

#include "engine/task/particle_entity_init.h"

namespace lotus
{
    Particle::Particle(Engine* _engine) : RenderableEntity(_engine)
    {
    }

    void Particle::Init(const std::shared_ptr<Particle>& sp, duration _lifetime)
    {
        lifetime = _lifetime;
        spawn_time = engine->getSimulationTime();
        engine->worker_pool.addWork(std::make_unique<ParticleEntityInitTask>(sp));
    }

    void Particle::tick(time_point time, duration delta)
    {
        if (time > (spawn_time + lifetime))
        {
            remove = true;
        }
        else
        {
            if (billboard)
            {
                auto& camera = engine->camera;
                rot_mat = glm::transpose(glm::mat3(engine->camera->getViewMatrix()));
            }
            RenderableEntity::tick(time, delta);
        }
    }
}