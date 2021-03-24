#pragma once

#include <unordered_map>
#include "engine/renderer/model.h"
#include "engine/renderer/texture.h"

class FFXIGame;
namespace FFXI
{
    class Dat;
    class Generator;
    class Keyframe;
    class Scheduler;
    class DatChunk;
}

class SystemDat
{
    struct _private_tag { explicit _private_tag() = default; };
public:
    static lotus::Task<std::unique_ptr<SystemDat>> Load(FFXIGame* game);

    std::unordered_map<std::string, FFXI::Generator*> generators;
    std::unordered_map<std::string, FFXI::Scheduler*> schedulers;

    SystemDat(FFXIGame* game, _private_tag);
private:
    FFXIGame* game{ nullptr };

    SystemDat(const SystemDat&) = delete;
    SystemDat(SystemDat&&) = default;
    SystemDat& operator=(const SystemDat&) = delete;
    SystemDat& operator=(SystemDat&&) = default;

    void ParseDir(FFXI::DatChunk*, std::vector<lotus::Task<std::shared_ptr<lotus::Texture>>>& texture_tasks, std::vector<lotus::Task<>>& model_tasks);

    std::vector<std::shared_ptr<lotus::Model>> generator_models;
    std::unordered_map<std::string, FFXI::Keyframe*> keyframes;
};