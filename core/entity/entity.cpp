#include "entity.h"

namespace lotus
{
    Entity::Entity()
    {
    }

    void Entity::tick()
    {
        for (auto& component : components)
        {
            component->tick();
        }
    }

    void Entity::setPos(float x, float y, float z)
    {
        this->pos = glm::vec3(x, y, z);
        this->pos_mat = glm::translate(glm::mat4{ 1.f }, glm::vec3{ x, y, z });
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

}
