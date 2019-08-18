#include "entity.h"

namespace lotus
{
    Entity::Entity()
    {
    }

    void Entity::tick_all(time_point time, duration delta)
    {
        tick(time, delta);
        for (auto& component : components)
        {
            component->tick(time, delta);
        }
    }

    void Entity::setPos(glm::vec3 pos)
    {
        this->pos = pos;
        this->pos_mat = glm::translate(glm::mat4{ 1.f }, pos);
    }

    void Entity::setRot(float rot)
    {
        this->rot = rot;
        this->rot_mat = glm::rotate(glm::mat4{ 1.f }, rot, glm::vec3(0, 0, 1));
    }

    void Entity::setRot(glm::quat quat)
    {
        this->rot_mat = glm::toMat4(quat);
    }

    glm::vec3 Entity::getPos()
    {
        return this->pos;
    }

}
