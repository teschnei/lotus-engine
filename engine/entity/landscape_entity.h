#pragma once
#include "renderable_entity.h"
#include "component/component_rewrite_test/instanced_models_component.h"

namespace lotus
{
    class Scene;
    class LandscapeEntity : public RenderableEntity
    {
    public:
        explicit LandscapeEntity(Engine* _engine) : RenderableEntity(_engine) {}
        virtual void populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index) override;
        virtual WorkerTask<> ReInitWork() override;

        std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> instance_offsets; //pair of offset/count

        std::vector<std::shared_ptr<Model>> collision_models;
        std::shared_ptr<TopLevelAccelerationStructure> collision_as;

        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> command_buffers;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> shadowmap_buffers;
    protected:
        virtual Task<> render(Engine* engine, std::shared_ptr<Entity> sp) override;
        WorkerTask<> renderWork();
        WorkerTask<> InitWork(std::vector<Test::InstancedModelsComponent::InstanceInfo>&&, Scene* scene);
    };
}
