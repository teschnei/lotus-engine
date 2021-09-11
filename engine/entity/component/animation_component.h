#pragma once
#include "component.h"
#include <memory>
#include <string>
#include <optional>
#include "engine/worker_task.h"
#include "engine/renderer/memory.h"
#include "engine/renderer/acceleration_structure.h"
#include "engine/renderer/animation.h"
#include "engine/renderer/skeleton.h"
#include "engine/renderer/model.h"

namespace lotus
{
    class Skeleton;
    class AnimationComponent : public Component
    {
    public:
        explicit AnimationComponent(Entity*, Engine* engine, std::unique_ptr<Skeleton>&&);
        virtual ~AnimationComponent() override = default;

        virtual Task<> tick(time_point time, duration delta) override;
        virtual Task<> render(Engine* engine, std::shared_ptr<Entity> sp) override;
        void playAnimation(std::string name, float speed = 1.f, std::optional<std::string> next_anim = {});
        void playAnimation(std::string name, duration anim_duration, std::optional<std::string> next_anim = {});
        void playAnimationLoop(std::string name, float speed = 1.f, uint8_t repetitions = 0);
        void playAnimationLoop(std::string name, duration anim_duration, uint8_t repetitions = 0);

        //acceleration structures (per model)
        std::vector<ModelTransformedGeometry> transformed_geometries;
        std::unique_ptr<Skeleton> skeleton;
        struct BufferBone
        {
            glm::vec4 rot;
            glm::vec3 trans;
            float _pad1;
            glm::vec3 scale;
            float _pad2;
        };
        std::unique_ptr<Buffer> skeleton_bone_buffer;

    protected:
        void changeAnimation(std::string name, float speed);
        static constexpr duration interpolation_time{ 100ms };

        WorkerTask<> renderWork();

        Animation* current_animation{ nullptr };
        time_point animation_start;
        std::optional<std::string> next_anim;
        std::vector<Skeleton::Bone> bones_interpolate;
        float anim_speed{ 1.f };
        bool loop{ true };
        uint8_t repetitions{ 0 };
    };
}
