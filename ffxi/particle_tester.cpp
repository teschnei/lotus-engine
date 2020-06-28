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

#include "fs.h"

ParticleTester::ParticleTester(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Input* _input) : InputComponent(_entity, _engine, _input),
    parser_system( static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path + fs::path2str("/ROM/0/0.DAT"), engine->renderer.RaytraceEnabled() ),
    parser( static_cast<FFXIConfig*>(engine->config.get())->ffxi.ffxi_install_path + fs::path2str("/ROM/10/9.DAT"), engine->renderer.RaytraceEnabled() )
{
    ParseDir(parser.root.get());
}

bool ParticleTester::handleInput(const SDL_Event& event)
{
    if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
    {
        switch (event.key.keysym.scancode)
        {
        case SDL_SCANCODE_R:
            entity->addComponent<SchedulerComponent>(schedulers["tgt0"]);
            return true;
        }
    }
    return false;
}

void ParticleTester::ParseDir(FFXI::DatChunk* chunk)
{
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
                auto texture = lotus::Texture::LoadTexture<FFXI::DXT3Loader>(engine, dxt3->name, dxt3);
                texture_map[dxt3->name] = std::move(texture);
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
            models.push_back(lotus::Model::LoadModel<FFXI::D3MLoader>(engine, std::string(d3m->name, 4), d3m));
        }
    }
}
