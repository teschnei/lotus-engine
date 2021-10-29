#pragma once
#include "component.h"
#include <memory>
#include "engine/worker_task.h"
#include "engine/renderer/memory.h"
#include "deformed_mesh_component.h"
#include "physics_component.h"

namespace lotus::Test
{
    class DeformableRasterComponent : public Component<DeformableRasterComponent, Test::DeformedMeshComponent, Test::PhysicsComponent>
    {
    public:
        explicit DeformableRasterComponent(Entity*, Engine* engine, DeformedMeshComponent& animation, PhysicsComponent& physics);

        WorkerTask<> tick(time_point time, duration delta);
    protected:
        void drawModelsToBuffer(vk::CommandBuffer command_buffer);
        void drawShadowmapsToBuffer(vk::CommandBuffer command_buffer);
        void drawModels(vk::CommandBuffer command_buffer, bool transparency, bool shadowmap);
        void drawMesh(vk::CommandBuffer command_buffer, bool shadowmap, const Mesh& mesh, uint32_t material_index);
    };
}
