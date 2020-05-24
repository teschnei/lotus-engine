#include "free_flying_camera.h"

#include "component/free_flying_camera_component.h"
#include "engine/core.h"

namespace lotus
{
    FreeFlyingCamera::FreeFlyingCamera(Engine* engine) : Camera(engine)
    {
        
    }

    void FreeFlyingCamera::Init(const std::shared_ptr<FreeFlyingCamera>& sp)
    {
        Camera::Init(sp);
        Input* input = &engine->input;
        addComponent<FreeFlyingCameraComponent>(input);
    }
}
