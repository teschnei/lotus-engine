#include "particle_tester.h"

#include <glm/gtx/rotate_vector.hpp>

#include "ffxi.h"
#include "engine/entity/particle.h"

#include "dat/generator.h"
#include "dat/d3m.h"
#include "dat/dxt3.h"
#include "dat/scheduler.h"
#include "entity/actor.h"

#include "engine/entity/component/tick_component.h"
#include "engine/entity/component/animation_component.h"
#include "entity/component/scheduler_component.h"

ParticleTester::ParticleTester(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Input* _input) : InputComponent(_entity, _engine, _input)
{
}

lotus::Task<> ParticleTester::init()
{
    //WS/dance/quarry: id + 3400
    //magic: id + 2800
    //item: id + 4912
    //ja: id + 4412
    //mobskill: id + 3900
    //pet mobskill: id + 49391
    size_t index = 2800 + 144;
    const auto& dat = static_cast<FFXIGame*>(engine->game)->dat_loader->GetDat(index);
    scheduler_resources = co_await SchedulerResources::Load(static_cast<FFXIGame*>(engine->game), dat);
}

lotus::Task<> ParticleTester::tick(lotus::time_point time, lotus::duration delta)
{
    auto actor = static_cast<Actor*>(entity);
    if (add)
    {
        auto cast_scheduler = actor->scheduler_map["cabk"];
        casting_scheduler = co_await entity->addComponent<SchedulerComponent>(cast_scheduler, scheduler_resources.get());
        start_time = time;
        add = false;
        casting = true;
    }
    if (casting && time > start_time + 3s)
    {
        if (casting_scheduler)
        {
            casting_scheduler->cancel();
            casting_scheduler = nullptr;
        }
        co_await entity->addComponent<SchedulerComponent>(scheduler_resources->schedulers["main"], scheduler_resources.get());
        casting = false;
        finished = true;
    }
    if (finished && time > start_time + 7s)
    {
        actor->animation_component->playAnimationLoop("idl");
        finished = false;
    }
}

bool ParticleTester::handleInput(const SDL_Event& event)
{
    if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
    {
        switch (event.key.keysym.scancode)
        {
        case SDL_SCANCODE_R:
            add = true;
            return true;
        default:
            return false;
        }
    }
    return false;
}

