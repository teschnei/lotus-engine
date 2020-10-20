#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/third_person_boom_camera.h"

class ThirdPersonFFXIVCamera : public lotus::ThirdPersonBoomCamera
{
public:
    explicit ThirdPersonFFXIVCamera(lotus::Engine*, std::weak_ptr<Entity>&);
    static lotus::Task<std::shared_ptr<ThirdPersonFFXIVCamera>> Init(lotus::Engine* engine, std::weak_ptr<Entity>& focus);
};
