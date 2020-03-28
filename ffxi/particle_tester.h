#pragma once

#include <memory>
#include "engine/entity/renderable_entity.h"
#include "engine/renderer/model.h"

namespace lotus
{
    class Engine;
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
};
