#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/renderer/vulkan/vulkan_inc.h"

namespace lotus
{
    class RendererRaytrace;
    class RendererRasterization;
    class RendererHybrid;
    class WorkerThread;
    class RenderableEntity;
    class RenderableEntityInitTask;

    class RenderableEntityInitializer : public EntityInitializer
    {
    public:
        RenderableEntityInitializer(Entity* _entity, RenderableEntityInitTask* task);

        virtual void initEntity(RendererRaytrace* renderer, WorkerThread* thread) override;
        virtual void drawEntity(RendererRaytrace* renderer, WorkerThread* thread) override;

        virtual void initEntity(RendererRasterization* renderer, WorkerThread* thread) override;
        virtual void drawEntity(RendererRasterization* renderer, WorkerThread* thread) override;

        virtual void initEntity(RendererHybrid* renderer, WorkerThread* thread) override;
        virtual void drawEntity(RendererHybrid* renderer, WorkerThread* thread) override;
    private:
        RenderableEntityInitTask* task;
        vk::UniqueCommandBuffer command_buffer;
    };

}