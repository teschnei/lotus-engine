#include "input.h"

#include "core.h"
#include "entity/component/input_component.h"

namespace lotus
{
    Input::Input(Engine* _engine, SDL_Window* window) : engine(_engine)
    {
        if (window)
        {
            SDL_ShowCursor(SDL_FALSE);
            SDL_SetWindowGrab(window, SDL_TRUE);
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
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
                    engine->renderer.resized();
                break;
            case SDL_MOUSEMOTION:
            case SDL_KEYUP:
            case SDL_KEYDOWN:
                HandleInputEvent(event);
                break;
            default:
                break;
            }
        }
    }

    bool Input::HandleInputEvent(const SDL_Event& event)
    {
        for (auto& component : components)
        {
            if (component->handleInput(event))
            {
                return true;
            }
        }
        return false;
    }

    void Input::registerComponent(InputComponent* component)
    {
        components.insert(component);
    }

    void Input::deregisterComponent(InputComponent* component)
    {
        components.erase(component);
    }
}
