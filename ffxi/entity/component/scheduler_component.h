#pragma once

#include "engine/entity/component/component.h"

#include <string>
#include <map>

namespace FFXI
{
    class Scheduler;
    class Generator;
}

class SchedulerResources;

class SchedulerComponent : public lotus::Component
{
public:
    SchedulerComponent(lotus::Entity* entity, lotus::Engine* engine, FFXI::Scheduler* scheduler, SchedulerResources* resources);
    virtual ~SchedulerComponent() = default;
    virtual lotus::Task<> tick(lotus::time_point time, lotus::duration delta) override;
    void cancel();

protected:
    FFXI::Scheduler* scheduler;
    SchedulerResources* resources;
    lotus::time_point start_time;
    uint32_t stage{ 0 };
    bool finished{ false };
};
