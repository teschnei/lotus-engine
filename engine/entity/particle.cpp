#include "particle.h"
#include "engine/core.h"

#include "engine/task/particle_entity_init.h"
#include "engine/task/entity_render.h"

#include <glm/gtx/euler_angles.hpp>

namespace lotus
{
    Particle::Particle(Engine* _engine) : RenderableEntity(_engine)
    {
    }

    void Particle::Init(const std::shared_ptr<Particle>& sp, duration _lifetime)
    {
        lifetime = _lifetime;
        spawn_time = engine->getSimulationTime();
        engine->worker_pool->addForegroundWork(std::make_unique<ParticleEntityInitTask>(sp));
    }

    void Particle::tick(time_point time, duration delta)
    {
        if (time > (spawn_time + lifetime))
        {
            remove = true;
        }
        else
        {
            entity_rot_mat = glm::transpose(glm::eulerAngleXYZ(rot_euler.x, rot_euler.y, rot_euler.z));
            auto camera_mat = glm::mat4(glm::transpose(glm::mat3(engine->camera->getViewMatrix())));
            if (!billboard)
            {
                //non-billboard particles still billboard, but just on the y-axis only
                camera_mat[1] = glm::vec4(0, 1, 0, 0);
                camera_mat[2].y = 0;
            }
            rot_mat = camera_mat * entity_rot_mat;
            RenderableEntity::tick(time, delta);
        }
    }

    void Particle::render(Engine* engine, std::shared_ptr<Entity>& sp)
    {
        auto distance = glm::distance(engine->camera->getPos(), sp->getPos());
        auto re_sp = std::static_pointer_cast<RenderableEntity>(sp);
        engine->worker_pool->addForegroundWork(std::make_unique<EntityRenderTask>(re_sp));
    }
    
    void Particle::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            if (model->bottom_level_as)
            {
                //glm is column-major so we have to transpose the model matrix for Raytrace
                auto matrix = glm::mat3x4{ glm::transpose(getModelMatrix()) };
                engine->renderer->populateAccelerationStructure(as, model->bottom_level_as.get(), matrix, resource_index, static_cast<uint32_t>(Raytracer::ObjectFlags::Particle), 4);
            }
        }
    }
}