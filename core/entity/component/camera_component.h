#pragma once
#include "input_component.h"

namespace lotus
{
    class CameraComponent : public InputComponent
    {
    public:
        explicit CameraComponent(Entity*, Input*);
        virtual bool handleInput(const SDL_Event&) override;
        virtual void tick() override;
    };
}
