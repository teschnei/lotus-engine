#pragma once

#include "engine/renderer/renderer.h"
#include "engine/entity/third_person_boom_camera.h"

class ThirdPersonFFXICamera : public lotus::ThirdPersonBoomCamera
{
public:
    ThirdPersonFFXICamera();
    void Init(const std::shared_ptr<ThirdPersonFFXICamera>& sp, lotus::Engine* engine, std::weak_ptr<Entity>& focus);
};
