#include "scheduler_component.h"

#include "engine/core.h"
#include "engine/entity/deformable_entity.h"
#include "engine/entity/component/animation_component.h"

#include "dat/scheduler.h"
#include "dat/generator.h"
#include "entity/component/generator_component.h"

SchedulerComponent::SchedulerComponent(lotus::Entity* entity, lotus::Engine* engine, FFXI::Scheduler* scheduler) :
    Component(entity, engine), scheduler(scheduler), start_time(engine->getSimulationTime())
{
    for (const auto& chunk : scheduler->parent->children)
    {
        if (auto generator = dynamic_cast<FFXI::Generator*>(chunk.get()))
        {
            generators.insert(std::make_pair(std::string(generator->name, 4), generator));
        }
    }
}

lotus::Task<> SchedulerComponent::tick(lotus::time_point time, lotus::duration delta)
{
    auto frame_number = std::chrono::duration_cast<std::chrono::milliseconds>(time - start_time).count() * (60.f / 1000.f);
    auto [buffer, next_frame] = scheduler->getStage(stage);

    uint8_t type = *buffer;
    uint8_t length = *(buffer + 1) * sizeof(uint32_t);
    uint16_t delay = *(uint16_t*)(buffer + 4);
    uint16_t duration = *(uint16_t*)(buffer + 6);
    char* id = (char*)(buffer + 8);

    if (frame_number > next_frame)
    {
        switch (type)
        {
        case 0x00:
        {
            //end
            remove = true;
            break;
        }
        case 0x02:
        {
            //generator
            auto generator = generators[std::string(id, 4)];
            {
                auto real_duration = std::chrono::milliseconds((duration * 1000) / 60);
                co_await entity->addComponent<GeneratorComponent>(generator, real_duration);
            }
            break;
        }
        case 0x03:
        case 0x3C:
        {
            //scheduler
            break;
        }
        case 0x05:
        {
            //motion
            auto motion = std::string(id, 4);
            if (auto deformable = dynamic_cast<lotus::DeformableEntity*>(entity))
            {
                deformable->animation_component->playAnimation(motion);
            }
            break;
        }
        case 0x0b:
        case 0x53:
        {
            //sound effect
        }
        }
        stage++;
    }
    co_return;
}
