#pragma once
#include "component.h"
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "engine/renderer/memory.h"

namespace lotus::Component
{
    class PhysicsComponent : public Component<PhysicsComponent>
    {
    public:
        explicit PhysicsComponent(Entity*, Engine* engine);

        Task<> tick(time_point time, duration delta);
        Task<> init();

        std::tuple<vk::Buffer, size_t, size_t> getUniformBuffer(uint32_t image) const;

        glm::mat4 getModelMatrix() const { return model; }
        glm::mat4 getModelMatrixT() const { return modelT; }
        glm::mat4 getModelMatrixIT() const { return modelIT; }
        glm::mat4 getPrevModelMatrix() const { return model_prev; }

        glm::vec3 getPos() const { return pos; }
        glm::quat getRot() const { return rot; }
        glm::vec3 getScale() const { return scale; }

        void setPos(glm::vec3 pos);
        void setRot(glm::quat rot);
        void setScale(glm::vec3 scale);

    protected:
        struct UniformBufferObject {
            glm::mat4 model;
            glm::mat4 modelIT;
            glm::mat4 model_prev;
        };

        std::unique_ptr<Buffer> uniform_buffer;
        UniformBufferObject* uniform_buffer_mapped{ nullptr };

        bool should_update_matrix{ true };

        glm::vec3 pos{ 0.f };
        glm::quat rot{1.f, 0.f, 0.f, 0.f};
        glm::vec3 scale{ 1.f, 1.f, 1.f };

        glm::mat4 model{};
        glm::mat4 modelT{};
        glm::mat4 modelIT{};
        glm::mat4 model_prev{};
    };
}
