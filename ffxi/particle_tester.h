#pragma once

#include <memory>
#include "engine/renderer/model.h"
#include "dat/dat.h"
//#include "engine/entity/component/input_component.h"
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
}

/*
class ParticleTester : public lotus::InputComponent
{
public:
    explicit ParticleTester(lotus::Entity*, lotus::Engine*, lotus::Input*);
    lotus::Task<> init();
    virtual bool handleInput(const SDL_Event&) override;
    virtual lotus::Task<> tick(lotus::time_point time, lotus::duration delta);
private:
    std::vector<std::shared_ptr<lotus::Model>> models;
    std::unique_ptr<SchedulerResources> scheduler_resources;
    std::unordered_map<std::string, std::shared_ptr<lotus::Texture>> texture_map;
    bool add{ false };
    bool casting{ false };
    bool finished{ false };
    SchedulerComponent* casting_scheduler{ nullptr };
    lotus::time_point start_time;
};
*/
