#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/third_person_boom_camera.h"

class ThirdPersonFFXICamera : public lotus::ThirdPersonBoomCamera
{
public:
    explicit ThirdPersonFFXICamera(lotus::Engine* engine);
    std::vector<lotus::UniqueWork> Init(const std::shared_ptr<ThirdPersonFFXICamera>& sp, std::weak_ptr<Entity>& focus);
};
