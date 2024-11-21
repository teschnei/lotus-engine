#pragma once
#include "component.h"
#include "deformed_mesh_component.h"
#include "lotus/renderer/memory.h"
#include "lotus/worker_task.h"
#include "render_base_component.h"
#include <memory>

namespace lotus::Component
{
class DeformableRasterComponent : public Component<DeformableRasterComponent, After<DeformedMeshComponent, RenderBaseComponent>>
{
public:
    explicit DeformableRasterComponent(Entity*, Engine* engine, const DeformedMeshComponent& animation, const RenderBaseComponent& physics);

    WorkerTask<> tick(time_point time, duration elapsed);

protected:
    const DeformedMeshComponent& mesh_component;
    const RenderBaseComponent& base_component;
    void drawModelsToBuffer(vk::CommandBuffer command_buffer);
    void drawShadowmapsToBuffer(vk::CommandBuffer command_buffer);
    void drawModels(vk::CommandBuffer command_buffer, bool transparency, bool shadowmap);
    void drawMesh(vk::CommandBuffer command_buffer, bool shadowmap, const Mesh& mesh, uint32_t material_index);
};
} // namespace lotus::Component
