#pragma once
#include "component.h"
#include "instanced_models_component.h"
#include "lotus/renderer/memory.h"
#include "lotus/worker_task.h"
#include <memory>

namespace lotus
{
class Mesh;

namespace Component
{
class InstancedRasterComponent : public Component<InstancedRasterComponent, After<InstancedModelsComponent>>
{
public:
    explicit InstancedRasterComponent(Entity*, Engine* engine, InstancedModelsComponent& models);

    WorkerTask<> init();
    WorkerTask<> tick(time_point time, duration elapsed);

protected:
    InstancedModelsComponent& models_component;
    std::vector<vk::UniqueCommandBuffer> render_buffers;
    std::vector<vk::UniqueCommandBuffer> shadowmap_buffers;

    void drawModelsToBuffer(vk::CommandBuffer command_buffer, uint32_t image, uint32_t prev_image);
    void drawShadowmapsToBuffer(vk::CommandBuffer command_buffer, uint32_t image);
    void drawModels(vk::CommandBuffer command_buffer, bool transparency, bool shadowmap);
    void drawMesh(vk::CommandBuffer command_buffer, bool shadowmap, const Mesh& mesh, uint32_t material_index, uint32_t count);
};
} // namespace Component
} // namespace lotus
