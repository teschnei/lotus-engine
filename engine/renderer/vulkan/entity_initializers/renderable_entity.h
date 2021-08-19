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
        void initModel(RendererRaytrace* renderer, Engine* engine, Model& model, ModelTransformedGeometry& model_transform);

        virtual void initEntity(RendererRasterization* renderer, Engine* engine) override;
        virtual void drawEntity(RendererRasterization* renderer, Engine* engine) override;
        void initModel(RendererRasterization* renderer, Engine* engine, Model& model, ModelTransformedGeometry& model_transform);

        virtual void initEntity(RendererHybrid* renderer, Engine* engine) override;
        virtual void drawEntity(RendererHybrid* renderer, Engine* engine) override;
        void initModel(RendererHybrid* renderer, Engine* engine, Model& model, ModelTransformedGeometry& model_transform);
    private:
        void initModelWork(RendererRaytrace* renderer, vk::CommandBuffer command_buffer, DeformableEntity* entity, const Model& model, ModelTransformedGeometry& model_transform);
        void initModelWork(RendererRasterization* renderer, vk::CommandBuffer command_buffer, DeformableEntity* entity, const Model& model, ModelTransformedGeometry& model_transform);
        void initModelWork(RendererHybrid* renderer, vk::CommandBuffer command_buffer, DeformableEntity* entity, const Model& model, ModelTransformedGeometry& model_transform);

        vk::UniqueCommandBuffer command_buffer;
    };

}
