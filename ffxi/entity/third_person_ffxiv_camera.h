#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/entity.h"

namespace lotus
{
    class Scene;
}

class ThirdPersonFFXIVCamera
{
public:
    static lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, std::tuple<>>> Init(lotus::Engine* engine, lotus::Scene* scene, std::weak_ptr<lotus::Entity>& focus);
};
