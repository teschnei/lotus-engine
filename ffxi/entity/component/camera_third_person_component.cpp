#include "camera_third_person_component.h"
#include "engine/core.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/input.h"

namespace FFXI
{
    CameraThirdPersonComponent::CameraThirdPersonComponent(lotus::Entity* _entity, lotus::Engine* _engine, lotus::Component::CameraComponent& _camera,
        ActorComponent& _target, bool right_click_face) :
        Component(_entity, _engine), camera(_camera), target(_target), right_click(right_click_face ? Look::LookBoth : Look::LookCamera)
    {
    }

    lotus::Task<> CameraThirdPersonComponent::tick(lotus::time_point time, lotus::duration delta)
    {
        auto target_pos = target.getPos();
        //TODO: base this distance off a skeleton bind point
        glm::vec3 boom_source = target_pos + glm::vec3{0.f, -0.5f, 0.f};

        auto new_distance = co_await engine->renderer->raytrace_queryer->query(lotus::RaytraceQueryer::ObjectFlags::LevelCollision, boom_source, glm::vec3{ 1.f, 0.f, 0.f } * rot, 0.f, distance);
        glm::vec3 boom{ new_distance - 0.05f, 0.f, 0.f };
        camera.setPos((boom * rot) + boom_source);
        camera.setTarget(boom_source);
        co_return;
    }

    bool CameraThirdPersonComponent::handleInput(lotus::Input* input, const SDL_Event& event)
    {
        if (event.type == SDL_MOUSEMOTION)
        {
            if (look == Look::LookCamera)
            {
                static float speed = 0.005f;
                swivel(-event.motion.xrel * speed, event.motion.yrel * speed);
                return true;
            }
            else if (look == Look::LookBoth)
            {
                static float speed = 0.005f;
                swivel(-event.motion.xrel * speed, event.motion.yrel * speed);
                target.setRot(glm::angleAxis(rot_x + glm::pi<float>(), glm::vec3(0.f, 1.f, 0.f)), false);
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
                look = event.button.button == SDL_BUTTON_LEFT ? Look::LookCamera : right_click;
                look_pos = { event.button.x, event.button.y };
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
                SDL_WarpMouseInWindow(window, look_pos.x, look_pos.y);
            }
        }
        else if (event.type == SDL_MOUSEWHEEL)
        {
            if (event.wheel.y != 0)
            {
                setDistance(std::clamp(distance - event.wheel.y, 1.f, 10.f));
            }
        }
        return false;
    }

    void CameraThirdPersonComponent::setDistance(float _distance)
    {
        distance = _distance;
    }

    void CameraThirdPersonComponent::swivel(float x_offset, float y_offset)
    {
        rot_x = glm::mod(rot_x + x_offset, glm::pi<float>() * 2);
        rot_y = std::clamp(rot_y + y_offset, -((glm::pi<float>() / 2) - 0.01f), (glm::pi<float>() / 2) - 0.01f);
        glm::quat yaw = glm::angleAxis(rot_x, glm::vec3(0.f, 1.f, 0.f));
        glm::quat pitch = glm::angleAxis(rot_y, glm::vec3(0.f, 0.f, 1.f));
        rot = glm::normalize(pitch * yaw);
    }
}
