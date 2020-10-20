#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "camera.h"

namespace lotus
{
    class FreeFlyingCamera : public Camera
    {
    public:
        explicit FreeFlyingCamera(Engine*);
        static Task<std::shared_ptr<FreeFlyingCamera>> Init(Engine*);
    };
}
