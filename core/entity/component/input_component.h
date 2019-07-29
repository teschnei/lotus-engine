#pragma once
#include "component.h"
#include <SDL.h>

namespace lotus
{
    class Input;

    class InputComponent : public Component
    {
    public:
        explicit InputComponent(Entity*, Input*);
        virtual ~InputComponent() override;

        virtual bool handleInput(const SDL_Event&) = 0;

    private:
        Input* input;
    };
}

