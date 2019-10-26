#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <memory>

#include "component/component.h"
#include "../types.h"

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

        void tick_all(time_point time, duration delta);
        void render_all(Engine* engine, std::shared_ptr<Entity>& sp);

        template<typename T, typename... Args>
        void addComponent(Args&&... args)
        {
            components.push_back(std::make_unique<T>(this, std::forward<Args>(args)...));
        };

        template<typename T>
        T* getComponent()
        {
            for (const auto& component : components)
            {
                if (auto cast = dynamic_cast<T*>(component.get()))
                {
                    return cast;
                }
            }
            return nullptr;
        }

        void setPos(glm::vec3);
        void setRot(float rot);
        void setRot(glm::quat rot);

        glm::vec3 getPos();

    protected:
        virtual void tick(time_point time, duration delta){}
        virtual void render(Engine* engine, std::shared_ptr<Entity>& sp){}

        glm::vec3 pos{};
        float rot{};
        glm::mat4 pos_mat{ 1.f };
        glm::mat4 rot_mat{ 1.f };
        //glm::mat4 model_matrix {1.f};

    private:
        std::vector<std::unique_ptr<Component>> components;
    };
}
