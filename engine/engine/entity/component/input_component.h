#pragma once
#include "component.h"
#include <SDL2/SDL.h>

namespace lotus
{
    class Input;

    class InputComponent : public Component
    {
    public:
        explicit InputComponent(Entity*, Engine*, Input*);
        virtual ~InputComponent() override;

        virtual bool handleInput(const SDL_Event&) = 0;

    protected:
        Input* input;
    };
}

