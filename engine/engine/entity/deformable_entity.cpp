#include "deformable_entity.h"
#include "engine/entity/component/animation_component.h"
#include "engine/renderer/raytrace_query.h"

namespace lotus
{
    DeformableEntity::DeformableEntity(Engine* engine) : RenderableEntity(engine)
    {
    }

    void DeformableEntity::addSkeleton(std::unique_ptr<Skeleton>&& skeleton, size_t vertex_stride)
    {
        addComponent<AnimationComponent>(std::move(skeleton), vertex_stride);
        animation_component = getComponent<AnimationComponent>();
    }

    void DeformableEntity::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
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

    void DeformableEntity::update_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
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
}