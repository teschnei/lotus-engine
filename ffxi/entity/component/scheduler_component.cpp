#include "scheduler_component.h"

#include "engine/core.h"

#include "ffxi.h"
#include "dat/scheduler.h"
#include "scheduler_resources.h"
#include "entity/component/generator_component.h"

namespace FFXI
{
    SchedulerComponent::SchedulerComponent(lotus::Entity* _entity, lotus::Engine* _engine, ActorComponent& _actor, FFXI::Scheduler* _scheduler, SchedulerResources* _resources, FFXI::SchedulerComponent* _parent) :
        Component(_entity, _engine), actor(_actor), scheduler(_scheduler), resources(_resources), parent(_parent)
    {
        
    }

    lotus::Task<> SchedulerComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        if (finished)
        {
            //keep alive until all children have finished scheduling/generating (unless canceled)
            //if (componentsEmpty())
            remove();
            co_return;
        }
        auto frame_number = std::chrono::duration_cast<std::chrono::milliseconds>(time - start_time).count() * (60.f / 1000.f);
        auto [buffer, next_frame] = scheduler->getStage(stage);

        std::string current_scheduler;

        while (frame_number > next_frame && !finished)
        {
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
                        engine->game->scene->AddComponents(co_await GeneratorComponent::make_component(entity, engine, generator->second, real_duration, this));
                    }
                    else if (auto generator = actor.getGenerator(std::string(id, 4)))
                    {
                        engine->game->scene->AddComponents(co_await GeneratorComponent::make_component(entity, engine, generator, real_duration, this));
                    }
                    else if (auto system_generator = ffxigame->system_dat->generators.find(std::string(id, 4)); system_generator != ffxigame->system_dat->generators.end())
                    {
                        engine->game->scene->AddComponents(co_await GeneratorComponent::make_component(entity, engine, system_generator->second, real_duration, this));
                    }
                }
                break;
            }
            //I think 0x03 is target, 0x3C is caster
            case 0x03:
            case 0x3C:
            {
                if (auto scheduler = resources->schedulers.find(std::string(id, 4)); scheduler != resources->schedulers.end())
                {
                    engine->game->scene->AddComponents(co_await SchedulerComponent::make_component(entity, engine, actor, scheduler->second, resources, this));
                }
                else if (auto scheduler = actor.getScheduler(std::string(id, 4)))
                {
                    engine->game->scene->AddComponents(co_await SchedulerComponent::make_component(entity, engine, actor, scheduler, resources, this));
                }
                else if (auto system_scheduler = ffxigame->system_dat->schedulers.find(std::string(id, 4)); system_scheduler != ffxigame->system_dat->schedulers.end())
                {
                    engine->game->scene->AddComponents(co_await SchedulerComponent::make_component(entity, engine, actor, system_scheduler->second, resources, this));
                }
                break;
            }
            case 0x05:
            {
                //motion
                auto motion = std::string(id, 3);
                auto repetitions = *(buffer + 30);
                auto animation_duration = std::chrono::milliseconds(static_cast<int>(duration * (1000.f / 60.f)));
                actor.getAnimationComponent().playAnimationLoop(motion, animation_duration, repetitions);
                break;
            }
            //like scheduler, 0x0b is probably sound on target, 0x53 sound on caster
            case 0x0b:
            case 0x53:
            {
                //sound effect
                break;
            }
            case 0x1e:
            case 0x2d:
            {
                /*
                if (current_scheduler.empty())
                {
                    removeComponent([id](Component* c) {
                        auto generator = dynamic_cast<GeneratorComponent*>(c);
                        return generator && generator->getName() == std::string(id, 4);
                    });
                }
                else
                {
                    auto s = entity->getComponent<SchedulerComponent>([current_scheduler](SchedulerComponent* c) { return c->getName() == current_scheduler; });
                    if (s)
                    {
                        s->removeComponent([id](Component* c) {
                            auto generator = dynamic_cast<GeneratorComponent*>(c);
                            return generator && generator->getName() == std::string(id, 4);
                        });
                    }
                }
                */
                break;
            }
            case 0x5f:
            {
                /*
                current_scheduler = std::string(id, 4);
                auto s = entity->getComponent<SchedulerComponent>([current_scheduler](SchedulerComponent* c) { return c->getName() == current_scheduler; });
                s->cancel();
                */
            }
            break;
            }
            stage++;
            std::tie(buffer, next_frame) = scheduler->getStage(stage);
        }
        co_return;
    }

    void SchedulerComponent::cancel()
    {
        remove();
    }
}
