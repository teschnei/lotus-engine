#pragma once
#include "engine/entity/component/component.h"
#include "actor_component.h"
#include "engine/entity/component/animation_component.h"
#include <memory>

namespace FFXI
{
    class ModernThirdPersonInputComponent : public lotus::Component::Component<ModernThirdPersonInputComponent, lotus::Component::Before<ActorComponent, lotus::Component::AnimationComponent>>
    {
    public:
        explicit ModernThirdPersonInputComponent(lotus::Entity*, lotus::Engine* engine, ActorComponent& actor, lotus::Component::AnimationComponent& animation);

        lotus::Task<> tick(lotus::time_point time, lotus::duration delta);
        bool handleInput(lotus::Input*, const SDL_Event&);
    private:
        ActorComponent& actor;
        lotus::Component::AnimationComponent& animation_component;
        glm::vec3 moving{};
        glm::vec3 moving_prev{};
        glm::vec3 move_to_rot{};
        constexpr static glm::vec3 step_height { 0.f, -0.3f, 0.f };
    };
}
