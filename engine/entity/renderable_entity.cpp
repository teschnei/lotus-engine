#include "renderable_entity.h"
#include "engine/core.h"
#include "component/animation_component.h"
#include "engine/renderer/vulkan/entity_initializers/renderable_entity.h"

namespace lotus
{
    RenderableEntity::RenderableEntity(Engine* engine) : Entity(engine)
    {
    }

    RenderableEntity::~RenderableEntity()
    {
        if (uniform_buffer_mapped)
        {
            uniform_buffer->unmap();
        }
        if (mesh_index_buffer_mapped)
        {
            mesh_index_buffer->unmap();
        }
    }

    void RenderableEntity::setScale(float x, float y, float z)
    {
        this->scale = glm::vec3(x, y, z);
        this->scale_mat = glm::scale(glm::mat4{ 1.f }, scale);
    }

    void RenderableEntity::setScale(glm::vec3 scale)
    {
        this->scale = scale;
        this->scale_mat = glm::scale(glm::mat4{ 1.f }, scale);
    }

    glm::vec3 RenderableEntity::getScale()
    {
        return scale;
    }

    Task<> RenderableEntity::render(Engine* engine, std::shared_ptr<Entity> sp)
    {
        //TODO: check bounding box
        //if (glm::dot(engine->camera.getPos() - pos, engine->camera.getRotationVector()) > 0)
        //{
        co_await renderWork();
        //}
    }

    WorkerTask<> RenderableEntity::renderWork()
    {
        auto image_index = engine->renderer->getCurrentImage();
        updateUniformBuffer(image_index);

        if (engine->config->renderer.RasterizationEnabled())
        {
            engine->worker_pool->command_buffers.graphics_secondary.queue(*command_buffers[image_index]);
            if (!shadowmap_buffers.empty())
                engine->worker_pool->command_buffers.shadowmap.queue(*shadowmap_buffers[image_index]);
        }
        co_return;
    }

    void RenderableEntity::updateUniformBuffer(int image_index)
    {
        RenderableEntity::UniformBufferObject* ubo = reinterpret_cast<RenderableEntity::UniformBufferObject*>(uniform_buffer_mapped + (image_index * engine->renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject))));
        ubo->model = getModelMatrix();
        ubo->modelIT = glm::transpose(glm::inverse(glm::mat3(ubo->model)));
    }

    void RenderableEntity::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            if (model->bottom_level_as)
            {
                //glm is column-major so we have to transpose the model matrix for Raytrace
                auto matrix = glm::mat3x4{ glm::transpose(getModelMatrix()) };
                engine->renderer->populateAccelerationStructure(as, model->bottom_level_as.get(), matrix, model->resource_index, static_cast<uint32_t>(Raytracer::ObjectFlags::DynamicEntities), 0);
            }
        }
    }

    void RenderableEntity::update_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            if (model->bottom_level_as)
            {
                as->UpdateInstance(model->bottom_level_as->instanceid, glm::mat3x4{ getModelMatrix() });
            }
        }
    }

    glm::mat4 RenderableEntity::getModelMatrix()
    {
        return this->pos_mat * this->rot_mat * this->scale_mat;
    }

    WorkerTask<> RenderableEntity::InitWork()
    {
        //priority: 0
        auto initializer = std::make_unique<RenderableEntityInitializer>(this);
        engine->renderer->initEntity(initializer.get(), engine);
        engine->renderer->drawEntity(initializer.get(), engine);
        engine->worker_pool->gpuResource(std::move(initializer));
        co_return;
    }

    WorkerTask<> RenderableEntity::ReInitWork()
    {
        auto initializer = std::make_unique<RenderableEntityInitializer>(this);
        engine->renderer->drawEntity(initializer.get(), engine);
        engine->worker_pool->gpuResource(std::move(initializer));
        co_return;
    }
}
