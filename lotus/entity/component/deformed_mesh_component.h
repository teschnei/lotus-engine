#pragma once
#include "animation_component.h"
#include "component.h"
#include "lotus/renderer/model.h"
#include "lotus/renderer/vulkan/common/global_descriptors.h"
#include "lotus/worker_task.h"
#include "render_base_component.h"
#include <memory>
#include <vector>

namespace lotus::Component
{
class DeformedMeshComponent : public Component<DeformedMeshComponent, After<RenderBaseComponent>>
{
public:
    explicit DeformedMeshComponent(Entity*, Engine* engine, const RenderBaseComponent& base_component, const AnimationComponent& animation_component,
                                   std::vector<std::shared_ptr<Model>> models);

    WorkerTask<> init();
    WorkerTask<> tick(time_point time, duration elapsed);

    struct ModelInfo
    {
        std::shared_ptr<Model> model;
        std::vector<std::unique_ptr<GlobalDescriptors::MeshInfoBuffer::View>> mesh_infos;
        // transformed vertex buffers (per mesh, per render target)
        std::vector<std::vector<std::unique_ptr<Buffer>>> vertex_buffers;
    };

    std::span<const ModelInfo> getModels() const;
    WorkerTask<ModelInfo> initModel(std::shared_ptr<Model> model) const;
    void replaceModelIndex(ModelInfo&& transform, uint32_t index);

protected:
    const RenderBaseComponent& base_component;
    const AnimationComponent& animation_component;
    std::vector<ModelInfo> models;

    ModelInfo initModelWork(vk::CommandBuffer command_buffer, std::shared_ptr<Model> model) const;
};
} // namespace lotus::Component
