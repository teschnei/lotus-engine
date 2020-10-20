#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/renderer/vulkan/vulkan_inc.h"

namespace lotus
{
    class RendererRaytrace;
    class RendererRasterization;
    class RendererHybrid;
    class RenderableEntity;

    class RenderableEntityInitializer : public EntityInitializer
    {
    public:
        RenderableEntityInitializer(Entity* _entity);

        virtual void initEntity(RendererRaytrace* renderer, Engine* engine) override;
        virtual void drawEntity(RendererRaytrace* renderer, Engine* engine) override;

        virtual void initEntity(RendererRasterization* renderer, Engine* engine) override;
        virtual void drawEntity(RendererRasterization* renderer, Engine* engine) override;

        virtual void initEntity(RendererHybrid* renderer, Engine* engine) override;
        virtual void drawEntity(RendererHybrid* renderer, Engine* engine) override;
    private:
        vk::UniqueCommandBuffer command_buffer;
    };

}
