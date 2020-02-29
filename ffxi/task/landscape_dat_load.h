#pragma once

#include "engine/work_item.h"
#include "entity/landscape_entity.h"

class LandscapeDatLoad : public lotus::WorkItem
{
public:
    LandscapeDatLoad(const std::shared_ptr<FFXILandscapeEntity>& entity, const std::string& dat);
    virtual ~LandscapeDatLoad() override = default;
    virtual void Process(lotus::WorkerThread*) override;

private:
    std::shared_ptr<FFXILandscapeEntity> entity;
    std::string dat;
};
