#pragma once
#include "component.h"
#include <memory>
#include <string>
#include <optional>
#include "engine/renderer/memory.h"
#include "engine/renderer/acceleration_structure.h"
#include "engine/renderer/animation.h"
#include "engine/renderer/skeleton.h"

namespace lotus
{
    class Skeleton;
    class AnimationComponent : public Component
    {
    public:
        struct ModelTransformedGeometry
        {
            //transformed vertex buffers (per mesh, per render target)
            std::vector<std::vector<std::unique_ptr<Buffer>>> vertex_buffers;
            //acceleration structures (per render target)
            std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> bottom_level_as;
        };
        explicit AnimationComponent(Entity*, Engine* engine, std::unique_ptr<Skeleton>&&, size_t vertex_stride);
        virtual ~AnimationComponent() override = default;

        virtual void tick(time_point time, duration delta) override;
        virtual void render(Engine* engine, std::shared_ptr<Entity>& sp) override;
        void playAnimation(std::string name, float speed = 1.f, std::optional<std::string> next_anim = {});
        void playAnimationLoop(std::string name, float speed = 1.f );

        //acceleration structures (per model)
        std::vector<ModelTransformedGeometry> transformed_geometries;
        std::unique_ptr<Skeleton> skeleton;
        size_t vertex_stride;
        Animation* current_animation{ nullptr };
        time_point animation_start;
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

        std::optional<std::string> next_anim;
        std::vector<Skeleton::Bone> bones_interpolate;
        float anim_speed{ 1.f };
        bool loop{ true };
    };
}
