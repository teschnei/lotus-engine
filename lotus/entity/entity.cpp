#include "entity.h"
#include <ranges>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

namespace lotus
{
    Entity::Entity()
    { }

    void Entity::setSharedPtr(std::shared_ptr<Entity> ptr)
    {
        self_shared = ptr;
    }

    std::shared_ptr<Entity> Entity::getSharedPtr()
    {
        return self_shared.lock();
    }
}
