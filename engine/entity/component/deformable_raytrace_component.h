#pragma once
#include "component.h"
#include <memory>
#include "engine/worker_task.h"
#include "engine/renderer/memory.h"
#include "engine/renderer/acceleration_structure.h"
#include "deformed_mesh_component.h"
#include "physics_component.h"

namespace lotus::Component
{
    class DeformableRaytraceComponent : public Component<DeformableRaytraceComponent, DeformedMeshComponent, PhysicsComponent>
    {
    public:
        struct ModelAccelerationStructures
        {
            //mesh acceleration structures
            std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> blas;
        };

        explicit DeformableRaytraceComponent(Entity*, Engine* engine, DeformedMeshComponent& deformed, PhysicsComponent& physics);

        WorkerTask<> init();
        Task<> tick(time_point time, duration delta);

        WorkerTask<ModelAccelerationStructures> initModel(std::shared_ptr<Model> model, const DeformedMeshComponent::ModelTransformedGeometry& model_transform) const;
        void replaceModelIndex(ModelAccelerationStructures&& acceleration, uint32_t index);

    protected:
        std::vector<ModelAccelerationStructures> acceleration_structures;

        ModelAccelerationStructures initModelWork(vk::CommandBuffer command_buffer, const Model& model, const DeformedMeshComponent::ModelTransformedGeometry& model_transform) const;
    };
}
