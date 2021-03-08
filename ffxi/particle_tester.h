#pragma once

#include <memory>
#include "engine/entity/renderable_entity.h"
#include "engine/renderer/model.h"
#include "dat/dat_parser.h"
#include "engine/entity/component/input_component.h"

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

class ParticleTester : public lotus::InputComponent
{
public:
    explicit ParticleTester(lotus::Entity*, lotus::Engine*, lotus::Input*);
    lotus::Task<> init();
    virtual bool handleInput(const SDL_Event&) override;
    virtual lotus::Task<> tick(lotus::time_point time, lotus::duration delta);
private:
    std::vector<std::shared_ptr<lotus::Model>> models;
    FFXI::DatParser parser_system;
    FFXI::DatParser parser;
    std::map<std::string, FFXI::Generator*> generators;
    std::map<std::string, FFXI::Scheduler*> schedulers;
    std::map<std::string, FFXI::Keyframe*> keyframes;
    std::unordered_map<std::string, std::shared_ptr<lotus::Texture>> texture_map;
    lotus::Task<> ParseDir(FFXI::DatChunk*);
    bool add{ false };
};
