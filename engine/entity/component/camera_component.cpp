#include "camera_component.h"
#include "engine/core.h"

namespace lotus::Component
{
    CameraComponent::CameraComponent(Entity* _entity, Engine* _engine) :
         Component(_entity, _engine)
    {
    }

    Task<> CameraComponent::init()
    {
        co_return;
    }

    Task<> CameraComponent::tick(time_point time, duration delta)
    {
        if (update_view)
        {
            view = glm::lookAt(pos, target, glm::vec3(0.f, 1.f, 0.f));
            view_inverse = glm::inverse(view);
            update_view = false;
        }
        if (update_projection)
        {
            projection = glm::perspective(fov, aspect_ratio, near_clip, far_clip);
            projection_inverse = glm::inverse(projection);
            update_projection = false;
        }
        co_return;
    }

    void CameraComponent::setPos(glm::vec3 _pos)
    {
        if (pos != _pos)
        {
            pos = _pos;
            update_view = true;
        }
    }

    void CameraComponent::setTarget(glm::vec3 _target)
    {
        if (target != _target)
        {
            target = _target;
            update_view = true;
        }
    }

    void CameraComponent::setPerspective(float _fov, float _aspect_ratio, float _near_clip, float _far_clip)
    {
        fov = _fov;
        aspect_ratio = _aspect_ratio;
        near_clip = _near_clip;
        far_clip = _far_clip;
        update_projection = true;
    }

    glm::mat4 CameraComponent::getViewMatrix()
    {
        return view;
    }

    void CameraComponent::writeToBuffer(CameraData& buffer)
    {
        buffer.proj = projection;
        buffer.view = view;
        buffer.proj_inverse = projection_inverse;
        buffer.view_inverse = view_inverse;
        buffer.eye_pos = glm::vec4(pos, 1.0);
    }
}
