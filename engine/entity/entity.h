#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <memory>

#include "component/component.h"
#include "engine/types.h"
#include "engine/worker_pool.h"
#include "engine/worker_task.h"

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

        Task<> tick_all(time_point time, duration delta);
        Task<> render_all(Engine* engine, std::shared_ptr<Entity>& sp);
        virtual WorkerTask<> ReInitWork() { co_return; };

        template<typename T, typename... Args>
        Task<T*> addComponent(Args&&... args)
        {
            auto component = std::make_unique<T>(this, engine, std::forward<Args>(args)...);
            co_await component->init();
            auto comp_ptr = component.get();
            components.push_back(std::move(component));
            co_return comp_ptr;
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

        glm::mat4 getPosMat() const { return pos_mat; }
        glm::mat4 getRotMat() const { return rot_mat; }

        bool should_remove() { return remove; };

        void setSharedPtr(std::shared_ptr<Entity>);
        std::shared_ptr<Entity> getSharedPtr();

    protected:
        virtual Task<> tick(time_point time, duration delta) { co_return; }
        virtual Task<> render(Engine* engine, std::shared_ptr<Entity> sp){ co_return; }

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
        std::weak_ptr<Entity> self_shared;
    };
}
