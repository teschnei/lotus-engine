#include "scheduler_component.h"

#include "ffxi.h"
#include "scheduler_resources.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/component/animation_component.h"

#include "entity/actor.h"
#include "dat/scheduler.h"
#include "dat/generator.h"
#include "entity/component/generator_component.h"

SchedulerComponent::SchedulerComponent(lotus::Entity* entity, lotus::Engine* engine, FFXI::Scheduler* _scheduler, SchedulerResources* _resources) :
    Component(entity, engine), scheduler(_scheduler), resources(_resources), start_time(engine->getSimulationTime())
{
}

lotus::Task<> SchedulerComponent::tick(lotus::time_point time, lotus::duration delta)
{
    if (finished)
    {
        //keep alive until all children have finished scheduling/generating (unless canceled)
        if (componentsEmpty())
            remove = true;
        co_return;
    }
    auto frame_number = std::chrono::duration_cast<std::chrono::milliseconds>(time - start_time).count() * (60.f / 1000.f);
    auto [buffer, next_frame] = scheduler->getStage(stage);

    while (frame_number > next_frame && !finished)
    {
        auto actor = static_cast<Actor*>(entity);
        auto ffxigame = static_cast<FFXIGame*>(engine->game);

        uint8_t type = *buffer;
        uint8_t length = *(buffer + 1) * sizeof(uint32_t);
        uint16_t delay = *(uint16_t*)(buffer + 4);
        uint16_t duration = *(uint16_t*)(buffer + 6);
        char* id = (char*)(buffer + 8);

        switch (type)
        {
        case 0x00:
        {
            //end
            finished = true;
            break;
        }
        case 0x02:
        {
            //generator
            {
                auto real_duration = std::chrono::milliseconds((duration * 1000) / 60);
                if (auto generator = resources->generators.find(std::string(id, 4)); generator != resources->generators.end())
                {
                    co_await addComponent<GeneratorComponent>(generator->second, real_duration);
                }
                else if (auto generator = actor->generator_map.find(std::string(id, 4)); generator != actor->generator_map.end())
                {
                    co_await addComponent<GeneratorComponent>(generator->second, real_duration);
                }
                else if (auto system_generator = ffxigame->system_dat->generators.find(std::string(id, 4)); system_generator != ffxigame->system_dat->generators.end())
                {
                    co_await addComponent<GeneratorComponent>(system_generator->second, real_duration);
                }
                std::cout << std::endl;
            }
            break;
        }
        //I think 0x03 is target, 0x3C is caster
        case 0x03:
        case 0x3C:
        {
            if (auto scheduler = resources->schedulers.find(std::string(id, 4)); scheduler != resources->schedulers.end())
            {
                co_await addComponent<SchedulerComponent>(scheduler->second, resources);
            }
            else if (auto scheduler = actor->scheduler_map.find(std::string(id, 4)); scheduler != actor->scheduler_map.end())
            {
                co_await addComponent<SchedulerComponent>(scheduler->second, resources);
            }
            else if (auto system_scheduler = ffxigame->system_dat->schedulers.find(std::string(id, 4)); system_scheduler != ffxigame->system_dat->schedulers.end())
            {
                co_await addComponent<SchedulerComponent>(system_scheduler->second, resources);
            }
            std::cout << std::endl;
            break;
        }
        case 0x05:
        {
            //motion
            auto motion = std::string(id, 3);
            auto repetitions = *(buffer + 30);
            auto animation_duration = std::chrono::milliseconds(static_cast<int>(duration * (1000.f / 60.f)));
            if (auto deformable = dynamic_cast<lotus::DeformableEntity*>(entity))
            {
                deformable->animation_component->playAnimationLoop(motion, animation_duration, repetitions);
            }
            break;
        }
        //like scheduler, 0x0b is probably sound on target, 0x53 sound on caster
        case 0x0b:
        case 0x53:
        {
            //sound effect
        }
        }
        stage++;
        std::tie(buffer, next_frame) = scheduler->getStage(stage);
    }
    co_return;
}

void SchedulerComponent::cancel()
{
    remove = true;
}
