#include "free_flying_camera.h"

#include "component/free_flying_camera_component.h"
#include "engine/core.h"

namespace lotus
{
    FreeFlyingCamera::FreeFlyingCamera(Engine* engine) : Camera(engine)
    {
        Input* input = engine->input.get();
        addComponent<FreeFlyingCameraComponent>(input);
    }

    Task<std::shared_ptr<FreeFlyingCamera>> FreeFlyingCamera::Init(Engine* engine)
    {
        co_return std::make_shared<FreeFlyingCamera>(engine);
    }
}
