#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "camera.h"

namespace lotus
{
    class FreeFlyingCamera : public Camera
    {
    public:
        explicit FreeFlyingCamera(Engine*);
        std::vector<UniqueWork> Init(const std::shared_ptr<FreeFlyingCamera>& sp);
    };
}
