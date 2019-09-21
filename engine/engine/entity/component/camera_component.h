#pragma once
#include "input_component.h"
#include <glm/glm.hpp>

namespace lotus
{
    class CameraComponent : public InputComponent
    {
    public:
        explicit CameraComponent(Entity*, Input*);
        virtual bool handleInput(const SDL_Event&) override;
        virtual void tick(time_point time, duration delta) override;
    private:
        bool look{ false };
        //x/y of mouse when mouselook started
        int look_x{ 0 };
        int look_y{ 0 };
        glm::vec3 moving;
    };
}
