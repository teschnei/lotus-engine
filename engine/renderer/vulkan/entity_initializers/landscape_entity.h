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

        virtual void initEntity(RendererRaytrace*, Engine*) override;
        virtual void drawEntity(RendererRaytrace*, Engine*) override;

        virtual void initEntity(RendererRasterization*, Engine*) override;
        virtual void drawEntity(RendererRasterization*, Engine*) override;

        virtual void initEntity(RendererHybrid*, Engine*) override;
        virtual void drawEntity(RendererHybrid*, Engine*) override;
    private:
        void createBuffers(Renderer*, Engine*);
        void drawModel(Engine* engine, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout);
        void drawMesh(Engine* engine, vk::CommandBuffer buffer, const Mesh& mesh, uint32_t count, vk::PipelineLayout, uint32_t material_index);

        std::vector<LandscapeEntity::InstanceInfo> instance_info;
        std::unique_ptr<Buffer> staging_buffer;
        vk::UniqueCommandBuffer command_buffer;
    };

}
