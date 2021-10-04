#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/renderer/vulkan/vulkan_inc.h"
#include "engine/entity/landscape_entity.h"

namespace lotus
{
    class RendererRaytrace;
    class RendererRasterization;
    class RendererHybrid;

    class LandscapeEntityInitializer : public EntityInitializer
    {
    public:
        LandscapeEntityInitializer(LandscapeEntity* _entity, std::vector<LandscapeEntity::InstanceInfo>&& instance_info);

        virtual void initEntity(RendererRaytrace* renderer, Engine* engine) override;

        virtual void initEntity(RendererRasterization* renderer, Engine* engine) override;
        virtual void drawEntity(RendererRasterization* renderer, Engine* engine, vk::CommandBuffer buffer, uint32_t image) override;
        virtual void drawEntityShadowmap(RendererRasterization* renderer, Engine* engine, vk::CommandBuffer buffer, uint32_t image) override;

        virtual void initEntity(RendererHybrid* renderer, Engine* engine) override;
        virtual void drawEntity(RendererHybrid* renderer, Engine* engine, vk::CommandBuffer buffer, uint32_t image) override;
    private:
        void createBuffers(Renderer*, Engine*);
        void drawModel(Engine* engine, vk::CommandBuffer buffer, bool transparency, bool shadowmap, vk::PipelineLayout);
        void drawMesh(Engine* engine, vk::CommandBuffer buffer, const Mesh& mesh, uint32_t count, vk::PipelineLayout, uint32_t material_index, bool shadowmap);

        std::vector<LandscapeEntity::InstanceInfo> instance_info;
        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueCommandBuffer command_buffer;
    };

}
