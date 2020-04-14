#pragma once

#include "entity.h"

#include <memory>
#include "engine/renderer/model.h"
#include "engine/renderer/skeleton.h"

namespace lotus
{
    class AnimationComponent;
    class RenderableEntity : public Entity
    {
    public:

        struct UniformBufferObject {
            glm::mat4 model;
            glm::mat4 modelIT;
        };

        explicit RenderableEntity(Engine*);
        virtual ~RenderableEntity();
        virtual std::unique_ptr<WorkItem> recreate_command_buffers(std::shared_ptr<Entity>& sp) override;

        void setScale(float x, float y, float z);
        void setScale(glm::vec3 scale);

        glm::vec3 getScale();

        virtual void populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index);
        virtual void update_AS(TopLevelAccelerationStructure* as, uint32_t image_index);

        std::vector<std::shared_ptr<Model>> models;

        glm::mat4 getModelMatrix();

        std::unique_ptr<Buffer> uniform_buffer;
        uint8_t* uniform_buffer_mapped{ nullptr };
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> command_buffers;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> shadowmap_buffers;

        std::unique_ptr<Buffer> mesh_index_buffer;
        uint8_t* mesh_index_buffer_mapped{ nullptr };

    protected:
        virtual void render(Engine* engine, std::shared_ptr<Entity>& sp) override;
        glm::vec3 scale{ 1.f, 1.f, 1.f };
        glm::mat4 scale_mat{ 1.f };
    };
}
