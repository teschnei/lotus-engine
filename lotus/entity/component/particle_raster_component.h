#pragma once
#include "component.h"
#include "lotus/renderer/memory.h"
#include "lotus/worker_task.h"
#include "particle_component.h"
#include "render_base_component.h"
#include <memory>

namespace lotus::Component
{
class ParticleRasterComponent : public Component<ParticleRasterComponent, lotus::Component::After<ParticleComponent, RenderBaseComponent>>
{
public:
    explicit ParticleRasterComponent(Entity*, Engine* engine, const ParticleComponent& particle, const RenderBaseComponent& base);

    WorkerTask<> tick(time_point time, duration elapsed);

protected:
    const ParticleComponent& particle_component;
    const RenderBaseComponent& base_component;
    void drawModelsToBuffer(vk::CommandBuffer command_buffer);
    void drawModels(vk::CommandBuffer command_buffer, bool transparency);
    void drawMesh(vk::CommandBuffer command_buffer, const Mesh& mesh, uint32_t material_index, uint32_t pipeline_index);
};
} // namespace lotus::Component
