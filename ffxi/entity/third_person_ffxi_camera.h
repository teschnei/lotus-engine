#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/third_person_boom_camera.h"

class ThirdPersonFFXICamera : public lotus::ThirdPersonBoomCamera
{
public:
    explicit ThirdPersonFFXICamera(lotus::Engine* engine, std::weak_ptr<Entity>& focus);
    static lotus::Task<std::shared_ptr<ThirdPersonFFXICamera>> Init(lotus::Engine* engine, std::weak_ptr<Entity>& focus);
};
