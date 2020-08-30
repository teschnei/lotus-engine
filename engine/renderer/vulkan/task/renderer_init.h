#pragma once

#include "engine/work_item.h"

namespace lotus
{
    class RendererHybrid;
    class RendererHybridInitTask : public WorkItem
    {
    public:
        RendererHybridInitTask(RendererHybrid* renderer);
        virtual ~RendererHybridInitTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        vk::UniqueCommandBuffer command_buffer;
        RendererHybrid* renderer;
    };

    class RendererRasterization;
    class RendererRasterizationInitTask : public WorkItem
    {
    public:
        RendererRasterizationInitTask(RendererRasterization* renderer);
        virtual ~RendererRasterizationInitTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        vk::UniqueCommandBuffer command_buffer;
        RendererRasterization* renderer;
    };

    class RendererRaytrace;
    class RendererRaytraceInitTask : public WorkItem
    {
    public:
        RendererRaytraceInitTask(RendererRaytrace* renderer);
        virtual ~RendererRaytraceInitTask() override = default;
        virtual void Process(WorkerThread*) override;

    private:
        vk::UniqueCommandBuffer command_buffer;
        RendererRaytrace* renderer;
    };
}
