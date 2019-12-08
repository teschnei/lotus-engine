#pragma once

#include "engine/renderer/renderer.h"
#include "camera.h"

namespace lotus
{
    class ThirdPersonBoomCamera : public Camera
    {
    public:
        explicit ThirdPersonBoomCamera(Engine*);
        void Init(const std::shared_ptr<ThirdPersonBoomCamera>& sp, std::weak_ptr<Entity>& focus);

        void setDistance(float distance);
        void look(glm::vec3 eye_focus);
        void swivel(float x_offset, float y_offset);
        void setPos(glm::vec3 pos);

    protected:
        void tick(time_point time, duration delta) override;
        float distance {7.f};
        bool update_pos {false};
        std::weak_ptr<Entity> focus;
    };
}
