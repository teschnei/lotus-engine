#pragma once
#include "component.h"
#include <memory>
#include "engine/worker_task.h"
#include "engine/renderer/memory.h"
#include "engine/renderer/acceleration_structure.h"
#include "deformed_mesh_component.h"
#include "render_base_component.h"

namespace lotus::Component
{
    class DeformableRaytraceComponent : public Component<DeformableRaytraceComponent, After<DeformedMeshComponent, RenderBaseComponent>>
    {
    public:
        struct ModelAccelerationStructures
        {
            //mesh acceleration structures
            std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> blas;
        };

        explicit DeformableRaytraceComponent(Entity*, Engine* engine, const DeformedMeshComponent& deformed, const RenderBaseComponent& physics);

        WorkerTask<> init();
        Task<> tick(time_point time, duration delta);

        WorkerTask<ModelAccelerationStructures> initModel(std::shared_ptr<Model> model, const DeformedMeshComponent::ModelTransformedGeometry& model_transform) const;
        void replaceModelIndex(ModelAccelerationStructures&& acceleration, uint32_t index);

    protected:
        const DeformedMeshComponent& mesh_component;
        const RenderBaseComponent& base_component;
        std::vector<ModelAccelerationStructures> acceleration_structures;

        ModelAccelerationStructures initModelWork(vk::CommandBuffer command_buffer, const Model& model, const DeformedMeshComponent::ModelTransformedGeometry& model_transform) const;
    };
}
