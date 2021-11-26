#pragma once
#include "component.h"
#include <memory>
#include "engine/worker_task.h"
#include "engine/renderer/memory.h"
#include "engine/renderer/acceleration_structure.h"
#include "particle_component.h"
#include "render_base_component.h"

namespace lotus::Component
{
    class ParticleRaytraceComponent : public Component<ParticleRaytraceComponent, After<ParticleComponent, RenderBaseComponent>>
    {
    public:
        explicit ParticleRaytraceComponent(Entity*, Engine* engine, ParticleComponent& particle, RenderBaseComponent& base);

        Task<> tick(time_point time, duration delta);

    protected:
        const ParticleComponent& particle_component;
        const RenderBaseComponent& base_component;
    };
}
