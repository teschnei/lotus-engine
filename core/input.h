#pragma once

#include <SDL.h>
#include <set>

namespace lotus
{
    class Engine;
    class InputComponent;

    class Input
    {
    public:
        Input(Engine* engine, SDL_Window*);
        void GetInput();
        void registerComponent(InputComponent*);
        void deregisterComponent(InputComponent*);
    private:
        bool HandleInputEvent(const SDL_Event&);
        Engine* engine;
        std::set<InputComponent*> components;
    };
}
