#include "entity.h"
#include <ranges>
#include <glm/gtx/euler_angles.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

namespace lotus
{
    Entity::Entity(Engine* engine) : engine(engine)
    {
    }

    Task<> Entity::tick_all(time_point time, duration delta)
    {
        std::vector<decltype(components)::value_type::element_type*> components_p{components.size()};
        std::ranges::transform(components, components_p.begin(), [](auto& c) { return c.get(); });
        for (auto component : components_p)
        {
            co_await component->tick(time, delta);
        }
        components.erase(std::remove_if(components.begin(), components.end(), [](auto& component)
        {
            return component->removed();
        }), components.end());
        co_await tick(time, delta);
    }

    Task<> Entity::render_all(Engine* engine, std::shared_ptr<Entity>& sp)
    {
        for (auto& component : components)
        {
            co_await component->render(engine, sp);
        }
        co_await render(engine, sp);
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
