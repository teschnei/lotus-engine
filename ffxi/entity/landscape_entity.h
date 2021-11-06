#pragma once
#include "engine/entity/entity.h"
#include "engine/renderer/mesh.h"
#include "dat/mzb.h"

namespace lotus
{
    class Scene;
}

class FFXILandscapeEntity
{
public:
    static lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, std::tuple<>>> Init(lotus::Engine* engine, lotus::Scene* scene, size_t zoneid);
    //FFXI::QuadTree quadtree{glm::vec3{}, glm::vec3{}};
protected:
    static lotus::WorkerTask<> Load(std::shared_ptr<lotus::Entity> entity, lotus::Engine* engine, size_t zoneid, lotus::Scene* scene);
    //lotus::time_point create_time{lotus::sim_clock::now()};
    //std::vector<std::shared_ptr<lotus::Model>> water_models;
};

