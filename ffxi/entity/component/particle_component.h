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
    class ParticleComponent : public lotus::Component::Component<ParticleComponent, lotus::Component::Before<lotus::Component::ParticleComponent, lotus::Component::RenderBaseComponent>>
    {
    public:
        explicit ParticleComponent(lotus::Entity*, lotus::Engine* engine, lotus::Component::ParticleComponent& engine_particle,
            lotus::Component::RenderBaseComponent& base, std::weak_ptr<lotus::Entity> actor, FFXI::Generator* generator, size_t index);
        ~ParticleComponent();

        lotus::Task<> init();
        lotus::Task<> tick(lotus::time_point time, lotus::duration delta);

    protected:
        lotus::Component::ParticleComponent& particle_component;
        lotus::Component::RenderBaseComponent& base_component;
        float keyframeScale(float in, float progress, std::string name);
        lotus::LightID light{};

        std::weak_ptr<lotus::Entity> actor;
        FFXI::Generator* generator{};
        size_t index{};
        lotus::duration duration{};
        lotus::time_point start_time;
        std::map<std::string, FFXI::Keyframe*> keyframes;
        uint64_t generated{ 0 };

        uint16_t pos_flags{ 0 };
        glm::vec3 origin{};
        lotus::Skeleton::Bone* bone{ nullptr };
        glm::vec3 bone_offset{};

        glm::vec3 local_pos{};
        glm::vec3 local_rot{};
        glm::vec3 local_scale{};

        //creation parameters
        glm::vec3 dpos{};
        glm::vec3 drot{};
        glm::vec3 dscale{};
        glm::vec2 duv{};

        //keyframe animation
        std::string kf_x_pos;
        std::string kf_y_pos;
        std::string kf_z_pos;

        std::string kf_x_rot;
        std::string kf_y_rot;
        std::string kf_z_rot;

        std::string kf_x_scale;
        std::string kf_y_scale;
        std::string kf_z_scale;

        std::string kf_r;
        std::string kf_g;
        std::string kf_b;
        std::string kf_a;

        std::string kf_u;
        std::string kf_v;
    };
}
