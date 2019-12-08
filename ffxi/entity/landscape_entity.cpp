#include "landscape_entity.h"

#include "task/landscape_dat_load.h"
#include "engine/core.h"

void FFXILandscapeEntity::Init(const std::shared_ptr<FFXILandscapeEntity>& sp, const std::string& dat)
{
    engine->worker_pool.addWork(std::make_unique<LandscapeDatLoad>(sp, dat));
}
