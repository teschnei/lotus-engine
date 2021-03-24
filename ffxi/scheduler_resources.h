#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include "engine/renderer/model.h"
#include "engine/renderer/texture.h"

class FFXIGame;
namespace FFXI
{
    class Dat;
    class Generator;
    class Scheduler;
    class Keyframe;
    class DatChunk;
}

class SchedulerResources
{
    struct _private_tag { explicit _private_tag() = default; };
public:
    static lotus::Task<std::unique_ptr<SchedulerResources>> Load(FFXIGame* game, const FFXI::Dat& dat);

    std::unordered_map<std::string, FFXI::Generator*> generators;
    std::unordered_map<std::string, FFXI::Scheduler*> schedulers;

    SchedulerResources(FFXIGame* game, _private_tag);

private:
    FFXIGame* game{ nullptr };

    SchedulerResources(const SchedulerResources&) = delete;
    SchedulerResources(SchedulerResources&&) = default;
    SchedulerResources& operator=(const SchedulerResources&) = delete;
    SchedulerResources& operator=(SchedulerResources&&) = default;

    void ParseDir(FFXI::DatChunk*, std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>>& texture_tasks, std::vector<lotus::Task<>>& model_tasks);

    std::vector<std::shared_ptr<lotus::Model>> generator_models;
    std::unordered_map<std::string, FFXI::Keyframe*> keyframes;
};
