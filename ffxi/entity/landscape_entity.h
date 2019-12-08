#pragma once
#include "engine/entity/landscape_entity.h"

class FFXILandscapeEntity : public lotus::LandscapeEntity
{
public:
    FFXILandscapeEntity(lotus::Engine* engine) : LandscapeEntity(engine) {}
    void Init(const std::shared_ptr<FFXILandscapeEntity>& sp, const std::string& dat);
};
