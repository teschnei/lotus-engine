#pragma once

#include <memory>
#include "engine/renderer/model.h"
#include "dat/dat.h"
#include "engine/entity/component/component.h"
#include "scheduler_resources.h"

namespace lotus
{
    class Engine;
}

namespace FFXI
{
    class Generator;
    class Keyframe;
    class Scheduler;
    class ActorComponent;
    class SchedulerComponent;
}

class ParticleTester : public lotus::Component::Component<ParticleTester>
{
public:
    explicit ParticleTester(lotus::Entity*, lotus::Engine* engine, FFXI::ActorComponent& actor);
    lotus::Task<> init();
    lotus::Task<> tick(lotus::time_point time, lotus::duration delta);
    bool handleInput(lotus::Input*, const SDL_Event&);
private:
    FFXI::ActorComponent& actor;
    std::vector<std::shared_ptr<lotus::Model>> models;
    std::unique_ptr<SchedulerResources> scheduler_resources;
    std::unordered_map<std::string, std::shared_ptr<lotus::Texture>> texture_map;
    bool add{ false };
    bool casting{ false };
    bool finished{ false };
    FFXI::SchedulerComponent* casting_scheduler{ nullptr };
    lotus::time_point start_time;
};
