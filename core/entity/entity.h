#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <memory>

#include "component/component.h"

namespace lotus
{
    class Entity
    {
    public:
        Entity();
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;
        Entity(Entity&&) = default;
        Entity& operator=(Entity&&) = default;
        virtual ~Entity() = default;

        virtual void tick();

        template <typename T, typename... Args>
        void addComponent(Args... args)
        {
            components.push_back(std::make_unique<T>(this, args...));
        };

        void setPos(glm::vec3);
        void setRot(float rot);
        void setRot(glm::quat rot);

        glm::vec3 getPos();

    protected:

        glm::vec3 pos{};
        float rot{};
        glm::mat4 pos_mat{ 1.f };
        glm::mat4 rot_mat{ 1.f };
        //glm::mat4 model_matrix {1.f};

    private:
        std::vector<std::unique_ptr<Component>> components;
    };
}
