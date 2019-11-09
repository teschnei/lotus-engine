#pragma once

#include "engine/renderer/renderer.h"
#include "camera.h"

namespace lotus
{
    class FreeFlyingCamera : public Camera
    {
    public:
        FreeFlyingCamera();
        void Init(const std::shared_ptr<FreeFlyingCamera>& sp, Engine* engine);
    };
}
