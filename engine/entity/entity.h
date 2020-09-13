#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <memory>

#include "component/component.h"
#include "../types.h"
#include "engine/work_item.h"

namespace lotus
{
    class Engine;
    class Entity
    {
    public:
        explicit Entity(Engine*);
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;
        Entity(Entity&&) = default;
        Entity& operator=(Entity&&) = default;
        virtual ~Entity() = default;

        void tick_all(time_point time, duration delta);
        void render_all(Engine* engine, std::shared_ptr<Entity>& sp);
        virtual UniqueWork recreate_command_buffers(std::shared_ptr<Entity>&) { return {}; };

        template<typename T, typename... Args>
        void addComponent(Args&&... args)
        {
            components.push_back(std::make_unique<T>(this, engine, std::forward<Args>(args)...));
        };

        //Components added from within components of the same entity must use an alternate method
        template<typename T, typename... Args>
        void addNewComponent(Args&&... args)
        {
            new_components.push_back(std::make_unique<T>(this, engine, std::forward<Args>(args)...));
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
        void setRot(glm::quat rot);
        void setRot(glm::vec3 rot);

        glm::vec3 getPos();
        glm::quat getRot();
        glm::vec3 getRotEuler();

        bool should_remove() { return remove; };

    protected:
        virtual void tick(time_point time, duration delta){}
        virtual void render(Engine* engine, std::shared_ptr<Entity>& sp){}

        Engine* engine;
        glm::vec3 pos{0.f};
        //despite there being two rot members, populating the quaternion does not populate the euler angle rotation - must choose which to use
        //particles, camera will use euler angles, others can use quaternion
        glm::vec3 rot_euler{0.f};
        glm::quat rot{1.f, 0.f, 0.f, 0.f};
        glm::mat4 pos_mat{ 1.f };
        glm::mat4 rot_mat{ 1.f };
        //glm::mat4 model_matrix {1.f};

        //toggle when the entity is to be removed from the scene
        bool remove{ false };

    private:
        std::vector<std::unique_ptr<Component>> components;
        std::vector<std::unique_ptr<Component>> new_components;
    };
}
