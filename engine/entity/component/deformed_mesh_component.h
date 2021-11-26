#pragma once
#include "component.h"
#include <memory>
#include <vector>
#include "animation_component.h"
#include "engine/worker_task.h"
#include "engine/renderer/model.h"

namespace lotus::Component
{
    class DeformedMeshComponent : public Component<DeformedMeshComponent>
    {
    public:
        explicit DeformedMeshComponent(Entity*, Engine* engine, const AnimationComponent& animation_component, std::vector<std::shared_ptr<Model>> models);

        WorkerTask<> init();
        WorkerTask<> tick(time_point time, duration elapsed);

        struct ModelTransformedGeometry
        {
            //transformed vertex buffers (per mesh, per render target)
            std::vector<std::vector<std::unique_ptr<Buffer>>> vertex_buffers;
            uint16_t resource_index{ 0 };
        };

        const ModelTransformedGeometry& getModelTransformGeometry(size_t index) const;
        std::vector<std::shared_ptr<Model>> getModels() const;
        WorkerTask<ModelTransformedGeometry> initModel(std::shared_ptr<Model> model) const;
        void replaceModelIndex(std::shared_ptr<Model>, ModelTransformedGeometry&& transform, uint32_t index);

    protected:
        const AnimationComponent& animation_component;
        std::vector<ModelTransformedGeometry> model_transforms;
        std::vector<std::shared_ptr<Model>> models;

        ModelTransformedGeometry initModelWork(vk::CommandBuffer command_buffer, const Model& model) const;
    };
}
