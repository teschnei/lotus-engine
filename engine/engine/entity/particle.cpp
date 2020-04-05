#include "particle.h"
#include "engine/core.h"

#include "engine/task/particle_entity_init.h"
#include "engine/task/particle_render.h"

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
                entity_rot_mat = glm::transpose(glm::toMat4(rot));
                rot_mat = glm::mat4(glm::transpose(glm::mat3(engine->camera->getViewMatrix()))) * entity_rot_mat;
            }
            RenderableEntity::tick(time, delta);
        }
    }

    void Particle::render(Engine* engine, std::shared_ptr<Entity>& sp)
    {
        auto distance = glm::distance(engine->camera->getPos(), sp->getPos());
        auto length_squared = glm::pow(distance, 2);
        auto re_sp = std::static_pointer_cast<Particle>(sp);
        engine->worker_pool.addWork(std::make_unique<ParticleRenderTask>(re_sp, -length_squared));
    }
    
    void Particle::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            if (model->bottom_level_as)
            {
                VkGeometryInstance instance{};
                instance.transform = glm::mat3x4{ glm::transpose(getModelMatrix()) };
                instance.accelerationStructureHandle = model->bottom_level_as->handle;
                instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
                instance.mask = static_cast<uint32_t>(Raytracer::ObjectFlags::Particle);
                instance.instanceOffset = lotus::Renderer::shaders_per_group * 4;
                instance.instanceId = resource_index;
                model->bottom_level_as->instanceid = as->AddInstance(instance);
            }
        }
    }
}