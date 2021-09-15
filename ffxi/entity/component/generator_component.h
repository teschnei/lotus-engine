#pragma once

#include "engine/entity/component/component.h"

#include <map>
#include "dat/generator.h"
#include "engine/audio.h"

class GeneratorComponent : public lotus::Component
{
public:
    GeneratorComponent(lotus::Entity* entity, lotus::Engine* engine, FFXI::Generator* generator, lotus::duration duration, std::shared_ptr<lotus::Particle> parent = {}, glm::vec3 offset = {});
    virtual lotus::Task<> tick(lotus::time_point time, lotus::duration delta) override;
    std::string getName();

protected:
    FFXI::Generator* generator{};
    std::optional<lotus::AudioEngine::AudioInstance> sound;
    lotus::duration duration;
    std::shared_ptr<lotus::Particle> parent;
    glm::vec3 offset;
    lotus::time_point start_time;
    lotus::time_point generate_time;
    glm::vec3 gen_rot_add{ 0 };
    float gen_rot_add_cylinder{ 0 };
    uint64_t generated{ 0 };
    std::map<std::string, FFXI::Keyframe*> keyframes;
};
