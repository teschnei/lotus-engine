#pragma once
#include "mzb.h"
#include "mmb.h"
#include "texture.h"
#include "core/entity/renderable_entity.h"

namespace lotus {
    class Game;
}

class FFXILoadLandTest
{
public:
    FFXILoadLandTest(lotus::Game* game);

    std::shared_ptr<lotus::RenderableEntity> getLand();

private:
    lotus::Game* game;

    std::unique_ptr<FFXI::MZB> mzb;
    std::vector<std::unique_ptr<FFXI::MMB>> mmbs;
    std::vector<std::unique_ptr<FFXI::Texture>> textures;
};
