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
            glm::mat4 model_prev;
        };

        explicit RenderableEntity(Engine*);
        virtual ~RenderableEntity();
        virtual WorkerTask<> ReInitWork() override;

        void setScale(float x, float y, float z);
        void setScale(glm::vec3 scale);

        glm::vec3 getScale();

        virtual void populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index);
        virtual void update_AS(TopLevelAccelerationStructure* as, uint32_t image_index);

        std::vector<std::shared_ptr<Model>> models;

        glm::mat4 getModelMatrix();
        glm::mat4 getPrevModelMatrix();
        glm::mat4x4 getScaleMat() const { return scale_mat; }

        glm::vec2 uv_offset{};

        std::unique_ptr<Buffer> uniform_buffer;
        uint8_t* uniform_buffer_mapped{ nullptr };
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> command_buffers;
        std::vector<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>> shadowmap_buffers;

    protected:
        virtual Task<> render(Engine* engine, std::shared_ptr<Entity> sp) override;
        WorkerTask<> renderWork();
        void updateUniformBuffer(int image_index);
        WorkerTask<> InitWork();
        WorkerTask<> InitModel(std::shared_ptr<Model> model, ModelTransformedGeometry&);
        glm::vec3 scale{ 1.f, 1.f, 1.f };
        glm::mat4 scale_mat{ 1.f };

        glm::mat4 model_prev{};
    };
}
