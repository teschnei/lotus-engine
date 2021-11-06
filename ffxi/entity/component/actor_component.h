#pragma once
#include "engine/entity/component/component.h"
#include "engine/entity/component/physics_component.h"
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace FFXI
{
    class ActorComponent : public lotus::Component::Component<ActorComponent, lotus::Component::PhysicsComponent>
    {
    public:
        explicit ActorComponent(lotus::Entity*, lotus::Engine* engine, lotus::Component::PhysicsComponent& physics);

        lotus::Task<> tick(lotus::time_point time, lotus::duration delta);

        glm::vec3 getPos() const { return pos; }
        glm::quat getRot() const { return rot; }
        float getSpeed() const { return speed; }

        void setPos(glm::vec3 pos, bool interpolate = true);
        void setRot(glm::quat rot, bool interpolate = true);
        void setModelOffsetRot(glm::quat rot);
        void setSpeed(float _speed) { speed = _speed; }

    protected:
        glm::vec3 pos{};
        glm::quat rot{ 1.f, 0.f, 0.f, 0.f };
        glm::quat model_offset_rot{ 1.f, 0.f, 0.f, 0.f };
        float speed{ 4.f };
    };
}
