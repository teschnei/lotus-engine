#include "renderable_entity.h"
#include "engine/core.h"
#include "engine/task/entity_render.h"
#include "component/animation_component.h"

namespace lotus
{
    RenderableEntity::RenderableEntity(Engine* engine) : Entity(engine)
    {
    }

    void RenderableEntity::setScale(float x, float y, float z)
    {
        this->scale = glm::vec3(x, y, z);
        this->scale_mat = glm::scale(glm::mat4{ 1.f }, glm::vec3{ x, y, z });
    }

    void RenderableEntity::addSkeleton(std::unique_ptr<Skeleton>&& skeleton, size_t vertex_stride)
    {
        addComponent<AnimationComponent>(std::move(skeleton), vertex_stride);
        animation_component = getComponent<AnimationComponent>();
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
            BottomLevelAccelerationStructure* blas = nullptr;
            if (model->weighted)
            {
                blas = animation_component->transformed_geometries[i].bottom_level_as[image_index].get();
            }
            else if (model->bottom_level_as)
            {
                blas = model->bottom_level_as.get();
            }
            if (blas)
            {
                VkGeometryInstance instance{};
                instance.transform = glm::mat3x4{ glm::transpose(getModelMatrix()) };
                instance.accelerationStructureHandle = blas->handle;
                instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
                if (std::none_of(model->meshes.begin(), model->meshes.end(), [](auto& mesh)
                {
                    return mesh->has_transparency;
                }))
                {
                    //instance.flags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_NV;
                }
                instance.mask = static_cast<uint32_t>(Raytracer::ObjectFlags::DynamicEntities);
                instance.instanceOffset = 0;
                instance.instanceId = blas->resource_index;
                blas->instanceid = as->AddInstance(instance);
            }
        }
    }

    void RenderableEntity::update_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            BottomLevelAccelerationStructure* blas = nullptr;
            if (model->weighted)
            {
                blas = animation_component->transformed_geometries[i].bottom_level_as[image_index].get();
            }
            else if (model->bottom_level_as)
            {
                blas = model->bottom_level_as.get();
            }
            if (blas)
            {
                as->UpdateInstance(blas->instanceid, glm::mat3x4{ getModelMatrix() });
            }
        }
    }

    glm::mat4 RenderableEntity::getModelMatrix()
    {
        return this->pos_mat * this->rot_mat * this->scale_mat;
    }
}
