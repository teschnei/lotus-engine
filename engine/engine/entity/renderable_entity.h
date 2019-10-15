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
            alignas(16) glm::mat4 model;
        };

        RenderableEntity(std::unique_ptr<Skeleton> skeleton = {});
        virtual ~RenderableEntity() = default;

        void setScale(float x, float y, float z);

        virtual void render(Engine* engine, std::shared_ptr<RenderableEntity>& sp);
        virtual void populate_AS(TopLevelAccelerationStructure* as);
        virtual void update_AS(TopLevelAccelerationStructure* as);

        std::vector<std::shared_ptr<Model>> models;

        glm::mat4 getModelMatrix();

        std::unique_ptr<Buffer> uniform_buffer;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> command_buffers;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> shadowmap_buffers;
        AnimationComponent* animation_component;
        
    protected:
        glm::vec3 scale{ 1.f, 1.f, 1.f };
        glm::mat4 scale_mat{ 1.f };
    };
}
