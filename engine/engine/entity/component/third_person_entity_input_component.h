#pragma once
#include "engine/entity/component/input_component.h"
#include <glm/glm.hpp>
#include "third_person_camera_component.h"

namespace lotus
{
    class ThirdPersonEntityInputComponent : public InputComponent
    {
    public:
        explicit ThirdPersonEntityInputComponent(Entity*, Engine*, Input*);
        virtual bool handleInput(const SDL_Event&) override;
        virtual void tick(time_point time, duration delta) override;
    protected:
        glm::vec3 moving {0.f};
        ThirdPersonCameraComponent* camera {nullptr};
    };
}
