#pragma once

#include "engine/renderer/vulkan/renderer.h"
#include "camera.h"

namespace lotus
{
    class ThirdPersonBoomCamera : public Camera
    {
    public:
        explicit ThirdPersonBoomCamera(Engine*, std::weak_ptr<Entity>& focus);
        static Task<std::shared_ptr<ThirdPersonBoomCamera>> Init(Engine*, std::weak_ptr<Entity>& focus);

        void setDistance(float distance);
        float getDistance() const { return distance; }
        void look(glm::vec3 eye_focus);
        void swivel(float x_offset, float y_offset);
        void setPos(glm::vec3 pos);
        void update_camera() { update = true; }

    protected:
        void tick(time_point time, duration delta) override;
        float distance {7.f};
        std::weak_ptr<Entity> focus;
    };
}
