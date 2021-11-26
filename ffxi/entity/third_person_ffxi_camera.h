#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/entity.h"

class ThirdPersonFFXICamera
{
public:
    static lotus::Task<std::shared_ptr<lotus::Entity>> Init(lotus::Engine* engine, lotus::Scene* scene, std::weak_ptr<lotus::Entity>& focus);
};
