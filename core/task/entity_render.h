#pragma once
#include "../work_item.h"
#include "../entity/renderable_entity.h"

namespace lotus
{
    class EntityRenderTask : public WorkItem
    {
    public:
        EntityRenderTask(std::shared_ptr<RenderableEntity>& entity);

        virtual void Process(WorkerThread*) override;
    private:
        void updateUniformBuffer(WorkerThread* thread, int image_index, RenderableEntity* entity);
        std::shared_ptr<RenderableEntity> entity;
    };
}
