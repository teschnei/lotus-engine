#include "third_person_camera_component.h"
#include "engine/entity/third_person_boom_camera.h"
#include "engine/input.h"
#include <glm/gtx/vector_angle.hpp>

namespace lotus
{
    ThirdPersonCameraComponent::ThirdPersonCameraComponent(Entity* _entity, Engine* _engine, Input* _input, std::weak_ptr<Entity>& _focus) : InputComponent(_entity, _engine, _input), focus(_focus)
    {
    }

    bool ThirdPersonCameraComponent::handleInput(const SDL_Event& event)
    {
        auto camera = static_cast<ThirdPersonBoomCamera*>(entity);
        if (event.type == SDL_MOUSEMOTION)
        {
            if (look == Look::LookCamera || look == Look::LookBoth)
            {
                static float speed = 0.005f;
                camera->swivel(-event.motion.xrel * speed, -event.motion.yrel * speed);
                if (look == Look::LookBoth)
                {
                    auto focus_sp = focus.lock();
                    if (focus_sp)
                    {
                        auto focus_pos = focus_sp->getPos();
                        auto camera_pos = entity->getPos();
                        glm::vec2 dir = glm::normalize(glm::vec2{focus_pos.x, focus_pos.z} - glm::vec2{camera_pos.x, camera_pos.z});
                        auto angle = glm::orientedAngle(dir, glm::vec2(1.f, 0.f));
                        focus_sp->setRot(glm::angleAxis(-angle, glm::vec3{ 0.f, 1.f, 0.f }));
                    }
                }
                return true;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN)
        {
            if (event.button.button == SDL_BUTTON_RIGHT ||
                event.button.button == SDL_BUTTON_LEFT)
            {
                auto window = input->GetWindow();
                SDL_ShowCursor(SDL_FALSE);
                SDL_SetWindowGrab(window, SDL_TRUE);
                SDL_SetRelativeMouseMode(SDL_TRUE);
                look = event.button.button == SDL_BUTTON_LEFT ? Look::LookCamera : Look::LookBoth;
                look_x = event.button.x;
                look_y = event.button.y;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONUP)
        {
            if (event.button.button == SDL_BUTTON_RIGHT ||
                event.button.button == SDL_BUTTON_LEFT)
            {
                auto window = input->GetWindow();
                SDL_ShowCursor(SDL_TRUE);
                SDL_SetWindowGrab(window, SDL_FALSE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
                look = Look::NoLook;
                SDL_WarpMouseInWindow(window, look_x, look_y);
            }
        }
        return false;
    }

    void ThirdPersonCameraComponent::tick(time_point time, duration delta)
    {
        auto camera = static_cast<ThirdPersonBoomCamera*>(entity);
        auto focus_lock = focus.lock();
        if (focus_lock)
        {
            //if (auto dist2 = glm::distance2(camera->getPos(), focus_lock->getPos()); dist2 > max_camera_distance + 0.001f)
            //{
            //    //move towards focus
            //    //move = (focus_lock->getPos() - camera->getPos()) * (1 - sqrt(max_camera_distance / dist2));
            //    camera->setDistance(sqrt(max_camera_distance));
            //}
            //else if (dist2 < min_camera_distance - 0.001f)
            //{
            //    //move away from focus
            //    //move = (camera->getPos() - focus_lock->getPos()) * (sqrt(dist2 / min_camera_distance));
            //    camera->setDistance(sqrt(min_camera_distance));
            //}
            camera->look(focus_lock->getPos());
        }
        else
        {
            //destroy self as focus is no longer valid
        }
    }
}
