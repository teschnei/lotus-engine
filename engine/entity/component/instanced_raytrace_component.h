#pragma once
#include "component.h"
#include <memory>
#include "engine/worker_task.h"
#include "engine/renderer/memory.h"
#include "engine/renderer/acceleration_structure.h"
#include "instanced_models_component.h"

namespace lotus::Component
{
    class InstancedRaytraceComponent : public Component<InstancedRaytraceComponent, InstancedModelsComponent>
    {
    public:
        explicit InstancedRaytraceComponent(Entity*, Engine* engine, InstancedModelsComponent& models);

        Task<> tick(time_point time, duration delta);
    };
}
