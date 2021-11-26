#pragma once
#include "engine/entity/entity.h"
#include "engine/renderer/mesh.h"
#include "dat/mzb.h"

namespace lotus
{
    class Scene;
}

namespace FFXI
{
    class Generator;
}

class FFXIParticle
{
public:
    static lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, std::tuple<>>> Init(lotus::Engine* engine, lotus::Scene* scene, std::weak_ptr<lotus::Entity> parent, FFXI::Generator* generator, std::shared_ptr<lotus::Model> model, size_t index);
    //FFXI::QuadTree quadtree{glm::vec3{}, glm::vec3{}};
protected:
    static lotus::WorkerTask<> Load(std::shared_ptr<lotus::Entity> entity, lotus::Engine* engine, lotus::Scene* scene, std::weak_ptr<lotus::Entity> parent, FFXI::Generator* generator, std::shared_ptr<lotus::Model> model, size_t index);
    //lotus::time_point create_time{lotus::sim_clock::now()};
    //std::vector<std::shared_ptr<lotus::Model>> water_models;
};

