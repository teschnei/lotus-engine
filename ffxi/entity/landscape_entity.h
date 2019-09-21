#pragma once
#include "engine/entity/landscape_entity.h"

class FFXILandscapeEntity : public lotus::LandscapeEntity
{
public:
    FFXILandscapeEntity() {}
    void Init(const std::shared_ptr<FFXILandscapeEntity>& sp, lotus::Engine* engine, const std::string& dat);
};
