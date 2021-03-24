#include "particle.h"
#include "engine/core.h"

#include "engine/renderer/vulkan/entity_initializers/particle_entity.h"

#include <glm/gtx/euler_angles.hpp>

namespace lotus
{
    Particle::Particle(Engine* _engine, duration _lifetime, std::shared_ptr<Model> _model) : RenderableEntity(_engine), lifetime(_lifetime), spawn_time(engine->getSimulationTime())
    {
        models.push_back(std::move(_model));
    }

    Task<std::shared_ptr<Particle>> Particle::Init(Engine* engine, duration lifetime, std::shared_ptr<Model> model)
    {
        auto particle = std::make_shared<Particle>(engine, lifetime, model);
        co_await particle->Load();
        co_return std::move(particle);
    }

    WorkerTask<> Particle::Load()
    {
        co_await InitWork();
    }

    WorkerTask<> Particle::InitWork()
    {
        auto initializer = std::make_unique<ParticleEntityInitializer>(this);
        engine->renderer->initEntity(initializer.get(), engine);
        engine->renderer->drawEntity(initializer.get(), engine);
        engine->worker_pool->gpuResource(std::move(initializer));
        co_return;
    }

    WorkerTask<> Particle::ReInitWork()
    {
        auto initializer = std::make_unique<ParticleEntityInitializer>(this);
        engine->renderer->drawEntity(initializer.get(), engine);
        engine->worker_pool->gpuResource(std::move(initializer));
        co_return;
    }

    Task<> Particle::tick(time_point time, duration delta)
    {
        if (time > (spawn_time + lifetime))
        {
            remove = true;
        }
        else
        {
            if (billboard != Billboard::None)
            {
                entity_rot_mat = glm::eulerAngleXYZ(rot_euler.x, rot_euler.y, rot_euler.z);
                auto camera_mat = glm::mat4(glm::transpose(glm::mat3(engine->camera->getViewMatrix())));
                if (billboard == Billboard::Y)
                {
                    camera_mat[1] = glm::vec4(0, 1, 0, 0);
                    camera_mat[2].y = 0;
                }
                rot_mat = camera_mat * entity_rot_mat;
            }
            co_await RenderableEntity::tick(time, delta);
        }
        co_return;
    }

    Task<> Particle::render(Engine* engine, std::shared_ptr<Entity> sp)
    {
        co_await renderWork();
    }

    WorkerTask<> Particle::renderWork()
    {
        auto image_index = engine->renderer->getCurrentImage();
        updateUniformBuffer(image_index);

        if (engine->config->renderer.RasterizationEnabled())
        {
            engine->worker_pool->command_buffers.particle.queue(*command_buffers[image_index]);
        }
        co_return;
    }

    void Particle::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        if (scale.x != 0 && scale.y != 0 && scale.z != 0)
        {
            for (size_t i = 0; i < models.size(); ++i)
            {
                const auto& model = models[i];
                if (model->bottom_level_as)
                {
                    //glm is column-major so we have to transpose the model matrix for Raytrace
                    auto matrix = glm::mat3x4{ glm::transpose(getModelMatrix()) };
                    //engine->renderer->populateAccelerationStructure(as, model->bottom_level_as.get(), matrix, resource_index, static_cast<uint32_t>(Raytracer::ObjectFlags::Particle), billboard ? 6 : 4);
                    engine->renderer->populateAccelerationStructure(as, model->bottom_level_as.get(), matrix, resource_index, static_cast<uint32_t>(Raytracer::ObjectFlags::Particle), 4 );
                }
            }
        }
    }
}
