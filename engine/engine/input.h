#pragma once

#include <SDL2/SDL.h>
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
        void RegisterComponent(InputComponent*);
        void DeregisterComponent(InputComponent*);
        SDL_Window* GetWindow();
    private:
        bool HandleInputEvent(const SDL_Event&);
        Engine* engine;
        std::set<InputComponent*> components;
        SDL_Window* window;
        bool look{ false };
        //x/y of mouse when mouselook started
        int look_x{ 0 };
        int look_y{ 0 };
    };
}
