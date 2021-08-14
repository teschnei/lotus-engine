#pragma once
#include "config.h"
#include "system_dat.h"
#include "audio/ffxi_audio.h"
#include "dat/dat_loader.h"
#include "engine/core.h"
#include "engine/game.h"

namespace FFXI
{
    class Generator;
    class Scheduler;
}

class FFXIGame : public lotus::Game
{
public:
    FFXIGame(const lotus::Settings& settings);

    virtual lotus::Task<> entry() override;
    virtual lotus::Task<> tick(lotus::time_point, lotus::duration) override;

    std::unique_ptr<FFXI::DatLoader> dat_loader;
    std::unique_ptr<SystemDat> system_dat;
    std::unique_ptr<FFXI::Audio> audio;

private:
    std::shared_ptr<lotus::Texture> default_texture;
    std::unique_ptr<lotus::Scene> loading_scene;

    lotus::WorkerTask<> load_scene();
};
