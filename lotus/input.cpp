#include "input.h"

#include "core.h"
#include "game.h"
#include "lotus/renderer/vulkan/renderer.h"

namespace lotus
{
    Input::Input(Engine* _engine, SDL_Window* _window) : engine(_engine), window(_window)
    {
    }

    void Input::GetInput()
    {
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                engine->close();
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED)
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

    SDL_Window* Input::GetWindow()
    {
        return window;
    }
}
