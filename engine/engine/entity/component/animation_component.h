#pragma once
#include "component.h"
#include <memory>
#include <string>
#include "engine/renderer/memory.h"
#include "engine/renderer/acceleration_structure.h"
#include "engine/renderer/animation.h"

namespace lotus
{
    class Skeleton;
    class AnimationComponent : public Component
    {
    public:
        struct ModelAccelerationStructure
        {
            //transformed vertex buffers (per mesh, per render target)
            std::vector<std::vector<std::unique_ptr<Buffer>>> vertex_buffers;
            //acceleration structures (per render target)
            std::vector<std::unique_ptr<BottomLevelAccelerationStructure>> bottom_level_as;
        };
        explicit AnimationComponent(Entity*, Engine* engine, std::unique_ptr<Skeleton>&&, size_t vertex_stride);
        virtual ~AnimationComponent() override = default;

        virtual void tick(time_point time, duration delta) override;
        void playAnimation(std::string name);

        //acceleration structures (per model)
        std::vector<ModelAccelerationStructure> acceleration_structures;
        std::unique_ptr<Skeleton> skeleton;
        size_t vertex_stride;
        Animation* current_animation{ nullptr };
        time_point animation_start;

    protected:
        Engine* engine;
    };
}
