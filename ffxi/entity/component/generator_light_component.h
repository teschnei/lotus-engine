#pragma once
#include "engine/entity/component/component.h"
#include "engine/entity/component/particle_component.h"
#include "engine/entity/component/render_base_component.h"
#include "engine/light_manager.h"
#include "actor_component.h"

class SchedulerResources;

namespace FFXI
{
    class Generator;
    class Keyframe;
    class GeneratorLightComponent : public lotus::Component::Component<GeneratorLightComponent>
    {
    public:
        explicit GeneratorLightComponent(lotus::Entity*, lotus::Engine* engine, FFXI::Generator* generator);
        ~GeneratorLightComponent();

        lotus::Task<> init();
        lotus::Task<> tick(lotus::time_point time, lotus::duration delta);

    protected:
        glm::vec3 pos{};
        glm::vec3 dpos{};
        glm::vec4 colour{};
        lotus::LightID light{};

        FFXI::Generator* generator{};
    };
}
