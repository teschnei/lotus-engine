module;

#include "lotus/renderer/sdl_inc.h"
#include <set>

module lotus;

import :core.input;
import :renderer.vulkan.renderer;

namespace lotus
{
Input::Input(Engine* _engine, SDL_Window* _window) : engine(_engine), window(_window) {}

void Input::GetInput()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            engine->close();
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            engine->renderer->resized();
            break;
        default:
            HandleInputEvent(event);
            break;
        }
    }
}

bool Input::HandleInputEvent(const SDL_Event& event)
{
    if (engine->game->scene)
    {
        engine->game->scene->component_runners->handleInput(this, event);
    }
    return false;
}

SDL_Window* Input::GetWindow() { return window; }
} // namespace lotus
