#pragma once

#include <memory>
#include "engine/types.h"
#include "engine/idgenerator.h"

namespace lotus
{
    class Entity final
    {
    public:
        explicit Entity();
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;
        Entity(Entity&&) = default;
        Entity& operator=(Entity&&) = default;
        ~Entity() = default;

        static uint32_t ID() { return IDGenerator<Entity, uint32_t>::template GetNewID<Entity>(); }

        bool should_remove() { return remove; };

        void setSharedPtr(std::shared_ptr<Entity>);
        std::shared_ptr<Entity> getSharedPtr();

    private:
        //toggle when the entity is to be removed from the scene
        bool remove{ false };
        std::weak_ptr<Entity> self_shared;
    };
}
