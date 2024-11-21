#pragma once

#include "lotus/renderer/sdl_inc.h"
#include <set>

namespace lotus
{
class Engine;

class Input
{
public:
    Input(Engine* engine, SDL_Window*);
    void GetInput();
    SDL_Window* GetWindow();

private:
    bool HandleInputEvent(const SDL_Event&);
    Engine* engine;
    SDL_Window* window;
    bool look{false};
    // x/y of mouse when mouselook started
    int look_x{0};
    int look_y{0};
};
} // namespace lotus
