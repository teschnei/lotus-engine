#include "deformable_entity.h"
#include "engine/core.h"
#include "engine/entity/component/animation_component.h"
#include "engine/renderer/raytrace_query.h"

namespace lotus
{
    DeformableEntity::DeformableEntity(Engine* engine) : RenderableEntity(engine)
    {
    }

    Task<> DeformableEntity::addSkeleton(std::unique_ptr<Skeleton>&& skeleton)
    {
        animation_component = co_await addComponent<AnimationComponent>(std::move(skeleton));
    }

    void DeformableEntity::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            BottomLevelAccelerationStructure* blas = nullptr;
            uint32_t resource_index = 0;
            if (model->weighted)
            {
                blas = animation_component->transformed_geometries[i].bottom_level_as[image_index].get();
                resource_index = animation_component->transformed_geometries[i].resource_index;
            }
            else if (model->bottom_level_as)
            {
                blas = model->bottom_level_as.get();
                resource_index = model->resource_index;
            }
            if (blas)
            {
                auto matrix = glm::mat3x4{ glm::transpose(getModelMatrix()) };
                engine->renderer->populateAccelerationStructure(as, blas, matrix, resource_index, static_cast<uint32_t>(RaytraceQueryer::ObjectFlags::DynamicEntities), 0);
            }
        }
    }

    Task<> DeformableEntity::render(Engine* engine, std::shared_ptr<Entity> sp)
    {
        co_await renderWork();
    }

    WorkerTask<> DeformableEntity::renderWork()
    {
        co_return;
    }
}
