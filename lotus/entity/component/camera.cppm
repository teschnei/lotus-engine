module;

#include <chrono>
#include <memory>

export module lotus:entity.component.camera;

import :util;
import :entity.component;
import :renderer.memory;
import glm;

export namespace lotus::Component
{
class CameraComponent : public Component<CameraComponent>
{
public:
    explicit CameraComponent(Entity*, Engine* engine);

    Task<> tick(time_point time, duration delta);
    Task<> init();

    // the camera updated something on this tick (for dependant components)
    bool updated();

    void setPos(glm::vec3 pos);
    void setTarget(glm::vec3 target);

    glm::vec3 getDir() { return glm::normalize(target - pos); }

    void setPerspective(float fov, float aspect_ratio, float near_clip, float far_clip);
    glm::vec3 getPos() const { return pos; }
    glm::mat4 getViewMatrix();
    glm::mat4 getProjMatrix();
    float getNearClip();
    float getFarClip();

    struct CameraData
    {
        glm::mat4 proj{};
        glm::mat4 view{};
        glm::mat4 proj_inverse{};
        glm::mat4 view_inverse{};
        glm::vec4 eye_pos{};
    };

    struct Frustum
    {
        glm::vec4 left;
        glm::vec4 right;
        glm::vec4 top;
        glm::vec4 bottom;
        glm::vec4 near;
        glm::vec4 far;
    } frustum{};

    void writeToBuffer(CameraData& buffer);

protected:
    bool update_view{true};
    glm::vec3 pos{0.f};
    glm::vec3 target{1.f};
    glm::mat4 view{};
    glm::mat4 view_inverse{};

    bool update_projection{true};
    float fov{};
    float aspect_ratio{};
    float near_clip{0.f};
    float far_clip{0.f};
    glm::mat4 projection{};
    glm::mat4 projection_inverse{};

    bool updated_tick{false};
};
} // namespace lotus::Component
