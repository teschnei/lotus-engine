#include "entity.h"
#include <glm/gtx/euler_angles.hpp>

namespace lotus
{
    Entity::Entity(Engine* engine) : engine(engine)
    {
    }

    void Entity::tick_all(time_point time, duration delta)
    {
        for (auto& component : components)
        {
            component->tick(time, delta);
        }
        components.erase(std::remove_if(components.begin(), components.end(), [](auto& component)
        {
            return component->removed();
        }), components.end());
        components.insert(components.end(), std::move_iterator(new_components.begin()), std::move_iterator(new_components.end()));
        new_components.clear();
        tick(time, delta);
    }

    void Entity::render_all(Engine* engine, std::shared_ptr<Entity>& sp)
    {
        for (auto& component : components)
        {
            component->render(engine, sp);
        }
        render(engine, sp);
    }

    void Entity::setPos(glm::vec3 pos)
    {
        this->pos = pos;
        this->pos_mat = glm::translate(glm::mat4{ 1.f }, pos);
    }

    void Entity::setRot(glm::quat quat)
    {
        this->rot = quat;
        this->rot_mat = glm::transpose(glm::toMat4(quat));
    }

    void Entity::setRot(glm::vec3 rot)
    {
        rot_euler = rot;
        this->rot_mat = glm::eulerAngleXYZ(rot.x, rot.z, rot.y);
    }

    glm::vec3 Entity::getPos()
    {
        return this->pos;
    }

    glm::quat Entity::getRot()
    {
        return this->rot;
    }
    glm::vec3 Entity::getRotEuler()
    {
        return rot_euler;
    }
}
