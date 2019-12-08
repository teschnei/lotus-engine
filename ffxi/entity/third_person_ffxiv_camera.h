#pragma once

#include "engine/renderer/renderer.h"
#include "engine/entity/third_person_boom_camera.h"

class ThirdPersonFFXIVCamera : public lotus::ThirdPersonBoomCamera
{
public:
    explicit ThirdPersonFFXIVCamera(lotus::Engine*);
    void Init(const std::shared_ptr<ThirdPersonFFXIVCamera>& sp, std::weak_ptr<Entity>& focus);
};
