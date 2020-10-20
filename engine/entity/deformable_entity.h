#pragma once

#include "renderable_entity.h"

namespace lotus
{
    class DeformableEntity : public RenderableEntity
    {
    public:
        explicit DeformableEntity(Engine*);
        virtual ~DeformableEntity() = default;

        void addSkeleton(std::unique_ptr<Skeleton>&& skeleton, size_t vertex_stride);

        virtual void populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index);
        virtual void update_AS(TopLevelAccelerationStructure* as, uint32_t image_index);

        AnimationComponent* animation_component {nullptr};
    protected:
        virtual Task<> render(Engine* engine, std::shared_ptr<Entity> sp) override;
        WorkerTask<> renderWork();
        void updateAnimationVertices(int image_index);
    };
}
