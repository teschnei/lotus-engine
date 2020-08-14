#include "deformable_entity.h"
#include "engine/core.h"
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
                auto matrix = glm::mat3x4{ glm::transpose(getModelMatrix()) };
                engine->renderer->populateAccelerationStructure(as, blas, matrix, blas->resource_index, static_cast<uint32_t>(Raytracer::ObjectFlags::DynamicEntities), 0);
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