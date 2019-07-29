#pragma once

#include "entity.h"

#include <memory>
#include "../renderer/model.h"
#include "../renderer/texture.h"

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

        std::shared_ptr<Model> model;
        std::shared_ptr<Texture> texture;

        glm::mat4 getModelMatrix();

        std::vector<std::unique_ptr<Buffer>> uniform_buffers;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> command_buffers;
        
    protected:
        glm::vec3 scale{ 1.f, 1.f, 1.f };
        glm::mat4 scale_mat{ 1.f };
    };
}
