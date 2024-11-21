#pragma once
#include "component.h"
#include "deformed_mesh_component.h"
#include "lotus/renderer/acceleration_structure.h"
#include "lotus/renderer/memory.h"
#include "lotus/worker_task.h"
#include "render_base_component.h"
#include <memory>

namespace lotus::Component
{
class DeformableRaytraceComponent : public Component<DeformableRaytraceComponent, After<DeformedMeshComponent, RenderBaseComponent>>
{
public:
    struct ModelAccelerationStructures
    {
        // mesh acceleration structures
        std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> blas;
    };

    explicit DeformableRaytraceComponent(Entity*, Engine* engine, const DeformedMeshComponent& deformed, const RenderBaseComponent& physics);

    WorkerTask<> init();
    Task<> tick(time_point time, duration delta);

    WorkerTask<ModelAccelerationStructures> initModel(const DeformedMeshComponent::ModelInfo& model_info) const;
    void replaceModelIndex(ModelAccelerationStructures&& acceleration, uint32_t index);

protected:
    const DeformedMeshComponent& mesh_component;
    const RenderBaseComponent& base_component;
    std::vector<ModelAccelerationStructures> acceleration_structures;

    ModelAccelerationStructures initModelWork(vk::CommandBuffer command_buffer, const DeformedMeshComponent::ModelInfo& model_info) const;
};
} // namespace lotus::Component
