#pragma once

#include "lotus/idgenerator.h"
#include "lotus/types.h"
#include <memory>

namespace lotus
{
class Scene;
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

    bool should_remove() { return removed; };
    void remove() { removed = true; }

    void setSharedPtr(std::shared_ptr<Entity>);
    std::shared_ptr<Entity> getSharedPtr();

private:
    // toggle when the entity is to be removed from the scene
    bool removed{false};
    std::weak_ptr<Entity> self_shared;
};
} // namespace lotus
