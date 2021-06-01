#pragma once
#include <filesystem>
#include <latch>
#include <atomic>
#include "engine/entity/deformable_entity.h"
#include "engine/task.h"
#include "dat/sk2.h"
#include "dat/scheduler.h"
#include "dat/generator.h"

namespace FFXI {
    class OS2;
}

//main FFXI entity class
class Actor : public lotus::DeformableEntity
{
public:
    explicit Actor(lotus::Engine* engine);
    static lotus::Task<std::shared_ptr<Actor>> Init(lotus::Engine* engine, size_t modelid);

    float speed{ 4.f };
    std::array<FFXI::SK2::GeneratorPoint, FFXI::SK2::GeneratorPointMax> generator_points{};
    std::map<std::string, FFXI::Scheduler*> scheduler_map;
    std::map<std::string, FFXI::Generator*> generator_map;
private:
    lotus::WorkerTask<> Load(size_t modelid);
};

class FFXIActorLoader
{
public:
    static lotus::Task<> LoadModel(std::shared_ptr<lotus::Model>, lotus::Engine* engine, const std::vector<FFXI::OS2*>& os2s, FFXI::SK2* sk2);
private:
    static void InitPipeline(lotus::Engine*);
    static inline vk::Pipeline pipeline;
    static inline vk::Pipeline pipeline_shadowmap;
    static inline std::latch pipeline_latch{ 1 };
    static inline std::atomic_flag pipeline_flag;
};
