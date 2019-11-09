#include "free_flying_camera.h"

#include "component/free_flying_camera_component.h"
#include "engine/core.h"

namespace lotus
{
    FreeFlyingCamera::FreeFlyingCamera() : Camera()
    {
        
    }

    void FreeFlyingCamera::Init(const std::shared_ptr<FreeFlyingCamera>& sp, Engine* engine)
    {
        Camera::Init(sp, engine);
        Input* input = &engine->input;
        addComponent<FreeFlyingCameraComponent>(input);
    }
}
