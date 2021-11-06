#include "free_flying_camera.h"

//#include "component/free_flying_camera_component.h"
#include "engine/core.h"

namespace lotus
{
    FreeFlyingCamera::FreeFlyingCamera(Engine* engine) : Camera(engine)
    {
    }

    Task<std::shared_ptr<FreeFlyingCamera>> FreeFlyingCamera::Init(Engine* engine)
    {
        auto camera = std::make_shared<FreeFlyingCamera>(engine);
        Input* input = engine->input.get();
 //       co_await camera->addComponent<FreeFlyingCameraComponent>(input);
        co_return camera;
    }
}
