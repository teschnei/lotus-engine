#include "particle_tester.h"

#include <glm/gtx/rotate_vector.hpp>

#include "engine/core.h"
#include "engine/game.h"
#include "engine/entity/particle.h"

#include "config.h"
#include "dat/generator.h"
#include "dat/d3m.h"
#include "dat/dxt3.h"
#include "dat/scheduler.h"

#include "engine/entity/component/tick_component.h"
#include "entity/component/scheduler_component.h"

ParticleTester::ParticleTester(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Input* _input) : InputComponent(_entity, _engine, _input),
    parser_system( static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path / "ROM/0/0.DAT", engine->config->renderer.RaytraceEnabled() ),
    parser( static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path / "ROM/10/9.DAT", engine->config->renderer.RaytraceEnabled() )
{
}

lotus::Task<> ParticleTester::init()
{
    co_await ParseDir(parser.root.get());
}

lotus::Task<> ParticleTester::tick(lotus::time_point time, lotus::duration delta)
{
    if (add)
    {
        co_await entity->addComponent<SchedulerComponent>(schedulers["tgt0"]);
        add = false;
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

lotus::Task<> ParticleTester::ParseDir(FFXI::DatChunk* chunk)
{
    std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>> texture_tasks;
    std::vector<lotus::Task<>> model_tasks;
    for (const auto& chunk : chunk->children)
    {
        ParseDir(chunk.get());
    }
    for (const auto& chunk : chunk->children)
    {
        if (auto generator = dynamic_cast<FFXI::Generator*>(chunk.get()))
        {
            generators.insert(std::make_pair(std::string(generator->name, 4), generator));
        }
        else if (auto dxt3 = dynamic_cast<FFXI::DXT3*>(chunk.get()))
        {
            if (dxt3->width > 0)
            {
                texture_tasks.push_back(lotus::Texture::LoadTexture<FFXI::DXT3Loader>(engine, dxt3->name, dxt3));
            }
        }
        else if (auto keyframe = dynamic_cast<FFXI::Keyframe*>(chunk.get()))
        {
            keyframes.insert(std::make_pair(std::string(keyframe->name, 4), keyframe));
        }
        else if (auto scheduler = dynamic_cast<FFXI::Scheduler*>(chunk.get()))
        {
            schedulers.insert(std::make_pair(std::string(scheduler->name, 4), scheduler));
        }
    }
    for (const auto& chunk : chunk->children)
    {
        if (auto d3m = dynamic_cast<FFXI::D3M*>(chunk.get()))
        {
            auto [model, model_task] = lotus::Model::LoadModel<FFXI::D3MLoader>(engine, std::string(d3m->name, 4), d3m);
            models.push_back(model);
            if (model_task)
                model_tasks.push_back(std::move(*model_task));
        }
    }
    //co_await all tasks
    for (const auto& task : texture_tasks)
    {
        co_await task;
    }
    for (const auto& task : model_tasks)
    {
        co_await task;
    }
}
