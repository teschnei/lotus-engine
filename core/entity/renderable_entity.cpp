#include "renderable_entity.h"
#include "core.h"
#include "task/entity_render.h"

namespace lotus
{
    RenderableEntity::RenderableEntity() : Entity()
    {
    }

    void RenderableEntity::setScale(float x, float y, float z)
    {
        this->scale = glm::vec3(x, y, z);
        this->scale_mat = glm::scale(glm::mat4{ 1.f }, glm::vec3{ x, y, z });
    }

    void RenderableEntity::render(Engine* engine, std::shared_ptr<RenderableEntity>& sp)
    {
        //TODO: check bounding box
        //if (glm::dot(engine->camera.getPos() - pos, engine->camera.getRotationVector()) > 0)
        {
            engine->worker_pool.addWork(std::make_unique<lotus::EntityRenderTask>(sp));
        }
    }

    void RenderableEntity::populate_AS(TopLevelAccelerationStructure* as)
    {
        for (const auto& model : models)
        {
            if (model->bottom_level_as)
            {
                VkGeometryInstance instance{};
                instance.transform = glm::mat3x4{ getModelMatrix() };
                instance.accelerationStructureHandle = model->bottom_level_as->handle;
                instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
                if (std::none_of(model->meshes.begin(), model->meshes.end(), [](auto& mesh)
                {
                    return mesh->has_transparency;
                }))
                {
                    //instance.flags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_NV;
                }
                instance.mask = 0xFF;
                instance.instanceOffset = 0;
                instance.instanceId = model->bottom_level_as->resource_index;
                model->acceleration_instanceid = as->AddInstance(instance);
            }
        }
    }

    void RenderableEntity::update_AS(TopLevelAccelerationStructure* as)
    {
        for (const auto& model : models)
        {
            if (model->bottom_level_as)
            {
                as->UpdateInstance(model->acceleration_instanceid, glm::mat3x4{ getModelMatrix() });
            }
        }
    }

    glm::mat4 RenderableEntity::getModelMatrix()
    {
        return this->pos_mat * this->rot_mat * this->scale_mat;
    }
}
