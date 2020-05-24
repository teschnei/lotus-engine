#include "renderable_entity.h"
#include "engine/core.h"
#include "engine/task/entity_render.h"
#include "component/animation_component.h"
#include "engine/task/renderable_entity_init.h"

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

    std::unique_ptr<WorkItem> RenderableEntity::recreate_command_buffers(std::shared_ptr<Entity>& sp)
    {
        return std::make_unique<RenderableEntityReInitTask>(std::static_pointer_cast<RenderableEntity>(sp));
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

    void RenderableEntity::render(Engine* engine, std::shared_ptr<Entity>& sp)
    {
        //TODO: check bounding box
        //if (glm::dot(engine->camera.getPos() - pos, engine->camera.getRotationVector()) > 0)
        {
            auto re_sp = std::static_pointer_cast<RenderableEntity>(sp);
            engine->worker_pool.addWork(std::make_unique<EntityRenderTask>(re_sp));
        }
    }

    void RenderableEntity::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            if (model->bottom_level_as)
            {
                vk::AccelerationStructureInstanceKHR instance{};
                //glm is column-major so we have to transpose the model matrix for Raytrace
                auto matrix = glm::mat3x4{ glm::transpose(getModelMatrix()) };
                memcpy(&instance.transform, &matrix, sizeof(matrix));
                instance.accelerationStructureReference = model->bottom_level_as->handle;
                instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable);
                instance.mask = static_cast<uint32_t>(Raytracer::ObjectFlags::DynamicEntities);
                instance.instanceShaderBindingTableRecordOffset = 0;
                instance.instanceCustomIndex = model->bottom_level_as->resource_index;
                model->bottom_level_as->instanceid = as->AddInstance(instance);
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
}
