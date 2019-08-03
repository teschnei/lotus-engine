#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "entity.h"

namespace lotus
{
    class Input;

    class Camera : public Entity
    {
    public:
        explicit Camera(Input* input);
        glm::mat4& getViewMatrix() { return view; }
        glm::mat4& getProjMatrix() { return proj; }

        void setPos(glm::vec3);
        void setPerspective(float radians, float aspect, float near, float far);
        void move(float forward_offset, float right_offset);
        void look(float rot_x_offset, float rot_y_offset);

        glm::vec3 getRotationVector() { return camera_rot; }

    private:

        float rot_x{0};
        float rot_z{ -glm::pi<float>()  };
        glm::vec3 camera_rot{};

        glm::mat4 view{};
        glm::mat4 proj{};
    };
}
