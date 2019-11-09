#pragma once

#include "engine/renderer/renderer.h"
#include "camera.h"

namespace lotus
{
    class ThirdPersonBoomCamera : public Camera
    {
    public:
        ThirdPersonBoomCamera();
        void Init(const std::shared_ptr<ThirdPersonBoomCamera>& sp, Engine* engine, std::weak_ptr<Entity>& focus);

        void setDistance(float distance);
        void look(glm::vec3 eye_focus);
        void swivel(float x_offset, float y_offset);
        void setPos(glm::vec3 pos);

    protected:
        void updatePos();
        float distance {7.f};
        std::weak_ptr<Entity> focus;
    };
}
