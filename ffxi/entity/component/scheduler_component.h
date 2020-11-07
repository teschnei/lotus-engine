#pragma once

#include "engine/entity/component/component.h"

#include <string>
#include <map>

namespace FFXI
{
    class Scheduler;
    class Generator;
}

class SchedulerComponent : public lotus::Component
{
public:
    SchedulerComponent(lotus::Entity* entity, lotus::Engine* engine, FFXI::Scheduler* scheduler);
    virtual ~SchedulerComponent() = default;
    virtual lotus::Task<> tick(lotus::time_point time, lotus::duration delta) override;

protected:
    FFXI::Scheduler* scheduler;
    lotus::time_point start_time;
    uint32_t stage{ 0 };
    std::map<std::string, FFXI::Generator*> generators;
};
