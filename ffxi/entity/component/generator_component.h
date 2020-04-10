#pragma once

#include "engine/entity/component/component.h"

#include <map>
#include "dat/generator.h"

class GeneratorComponent : public lotus::Component
{
public:
    GeneratorComponent(lotus::Entity* entity, lotus::Engine* engine, FFXI::Generator* generator, lotus::duration duration);
    virtual ~GeneratorComponent() = default;
    virtual void tick(lotus::time_point time, lotus::duration delta) override;

protected:
    FFXI::Generator* generator;
    lotus::duration duration;
    lotus::time_point start_time;
    lotus::time_point generate_time;
    glm::vec3 gen_rot_add{ 0 };
    uint64_t generated{ 0 };
    std::map<std::string, FFXI::Keyframe*> keyframes;
};
