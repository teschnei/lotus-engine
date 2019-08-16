#pragma once

#include "entity.h"

#include <memory>
#include "core/renderer/model.h"

namespace lotus
{
    class RenderableEntity : public Entity
    {
    public:

        struct UniformBufferObject {
            alignas(16) glm::mat4 model;
            alignas(16) glm::mat4 view;
            alignas(16) glm::mat4 proj;
        };

        RenderableEntity();
        virtual ~RenderableEntity() = default;

        void setScale(float x, float y, float z);

        virtual void render(Engine* engine, std::shared_ptr<RenderableEntity>& sp);

        std::vector<std::unique_ptr<Model>> models;

        glm::mat4 getModelMatrix();

        std::unique_ptr<Buffer> uniform_buffer;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> command_buffers;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> shadowmap_buffers;
        
    protected:
        glm::vec3 scale{ 1.f, 1.f, 1.f };
        glm::mat4 scale_mat{ 1.f };
    };
}
