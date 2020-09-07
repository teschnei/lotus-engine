#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "camera.h"

namespace lotus
{
    class FreeFlyingCamera : public Camera
    {
    public:
        explicit FreeFlyingCamera(Engine*);
        std::vector<std::unique_ptr<WorkItem>> Init(const std::shared_ptr<FreeFlyingCamera>& sp);
    };
}
