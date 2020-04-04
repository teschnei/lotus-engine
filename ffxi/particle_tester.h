#pragma once

#include <memory>
#include "engine/entity/renderable_entity.h"
#include "engine/renderer/model.h"
#include "dat/dat_parser.h"

namespace lotus
{
    class Engine;
}

namespace FFXI
{
    class Generator;
    class Keyframe;
}

class ParticleTester
{
public:
    explicit ParticleTester(lotus::Engine* engine);
    void tick(lotus::time_point time, lotus::time_point::duration);
private:
    lotus::time_point last;
    void CreateParticle();
    lotus::Engine* engine;
    std::vector<std::shared_ptr<lotus::Model>> models;
    FFXI::DatParser parser;
    FFXI::DatParser parser_system;
    std::map<std::string, FFXI::Generator*> generators;
    std::map<std::string, FFXI::Keyframe*> keyframes;
};
