#pragma once
#include "component.h"
#include "instanced_models_component.h"
#include "lotus/renderer/acceleration_structure.h"
#include "lotus/renderer/memory.h"
#include "lotus/worker_task.h"
#include <memory>

namespace lotus::Component
{
class InstancedRaytraceComponent : public Component<InstancedRaytraceComponent, After<InstancedModelsComponent>>
{
public:
    explicit InstancedRaytraceComponent(Entity*, Engine* engine, const InstancedModelsComponent& models);

    Task<> tick(time_point time, duration delta);

protected:
    const InstancedModelsComponent& models_component;
};
} // namespace lotus::Component
