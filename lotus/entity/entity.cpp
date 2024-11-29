module;

#include <memory>

module lotus;

import :entity;
import glm;

namespace lotus
{
Entity::Entity() {}

void Entity::setSharedPtr(std::shared_ptr<Entity> ptr) { self_shared = ptr; }

std::shared_ptr<Entity> Entity::getSharedPtr() { return self_shared.lock(); }
} // namespace lotus
