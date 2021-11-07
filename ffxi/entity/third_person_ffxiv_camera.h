#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "engine/entity/entity.h"
#include "component/actor_component.h"

namespace lotus
{
    class Scene;
}

class ThirdPersonFFXIVCamera
{
public:
    static lotus::Task<std::pair<std::shared_ptr<lotus::Entity>, std::tuple<lotus::Component::CameraComponent*>>> Init(lotus::Engine* engine, lotus::Scene* scene, FFXI::ActorComponent* actor_component);
};
