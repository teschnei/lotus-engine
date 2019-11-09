#pragma once

#include "engine/renderer/renderer.h"
#include "engine/entity/third_person_boom_camera.h"

class ThirdPersonFFXIVCamera : public lotus::ThirdPersonBoomCamera
{
public:
    ThirdPersonFFXIVCamera();
    void Init(const std::shared_ptr<ThirdPersonFFXIVCamera>& sp, lotus::Engine* engine, std::weak_ptr<Entity>& focus);
};
