#pragma once
#include "component.h"
#include "camera_component.h"

namespace lotus
{
    class Entity;
    class Engine;

    namespace Component
    {
        class CameraCascadesComponent : public Component<CameraCascadesComponent, After<CameraComponent>>
        {
        public:
            explicit CameraCascadesComponent(Entity*, Engine*, CameraComponent& camera);
            Task<> tick(time_point time, duration delta);
        protected:
            CameraComponent& camera;
        };
    }
}
