#pragma once
#include "engine/entity/landscape_entity.h"
#include "dat/mzb.h"

class FFXILandscapeEntity : public lotus::LandscapeEntity
{
public:
    FFXILandscapeEntity(lotus::Engine* engine) : LandscapeEntity(engine) {}
    void Init(const std::shared_ptr<FFXILandscapeEntity>& sp, const std::string& dat);
    virtual void populate_AS(lotus::TopLevelAccelerationStructure* as, uint32_t image_index) override;
    FFXI::QuadTree quadtree{glm::vec3{}, glm::vec3{}};
    std::vector<std::pair<uint32_t, InstanceInfo>> model_vec;
protected:
    //virtual void render(lotus::Engine* engine, std::shared_ptr<Entity>& sp) override;
};
