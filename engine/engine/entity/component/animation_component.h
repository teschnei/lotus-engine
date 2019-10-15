#pragma once
#include "component.h"
#include <memory>
#include <string>
#include "engine/renderer/memory.h"

namespace lotus
{
    class Skeleton;
    class AnimationComponent : public Component
    {
    public:
        explicit AnimationComponent(Entity*, std::unique_ptr<Skeleton>);
        virtual ~AnimationComponent() override = default;

        virtual void tick(time_point time, duration delta) override;
        void playAnimation(std::string name);

        std::unique_ptr<Buffer> vertex_buffer;

    protected:
        std::unique_ptr<Skeleton> skeleton;
    };
}
