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
        explicit DeformableRaytraceComponent(Entity*, Engine* engine, DeformedMeshComponent& deformed, PhysicsComponent& physics);

        WorkerTask<> init();
        Task<> tick(time_point time, duration delta);
    protected:
        struct ModelAccelerationStructures
        {
            //mesh acceleration structures
            std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> blas;
            uint16_t resource_index{ 0 };
        };

        std::vector<ModelAccelerationStructures> acceleration_structures;

        ModelAccelerationStructures initModelWork(vk::CommandBuffer command_buffer, const Model& model, const DeformedMeshComponent::ModelTransformedGeometry& model_transform);
    };
}
