#include "renderable_entity.h"
#include "engine/core.h"
#include "component/animation_component.h"
#include "engine/renderer/vulkan/entity_initializers/renderable_entity.h"

namespace lotus
{
    RenderableEntity::RenderableEntity(Engine* engine) : Entity(engine)
    {
    }

    RenderableEntity::~RenderableEntity()
    {
        if (uniform_buffer_mapped)
        {
            uniform_buffer->unmap();
        }
    }

    void RenderableEntity::setScale(float x, float y, float z)
    {
        this->scale = glm::vec3(x, y, z);
        this->scale_mat = glm::scale(glm::mat4{ 1.f }, scale);
    }

    void RenderableEntity::setScale(glm::vec3 scale)
    {
        this->scale = scale;
        this->scale_mat = glm::scale(glm::mat4{ 1.f }, scale);
    }

    glm::vec3 RenderableEntity::getScale()
    {
        return scale;
    }

    Task<> RenderableEntity::render(Engine* engine, std::shared_ptr<Entity> sp)
    {
        //TODO: check bounding box
        //if (glm::dot(engine->camera.getPos() - pos, engine->camera.getRotationVector()) > 0)
        //{
        co_await renderWork();
        //}
    }

    WorkerTask<> RenderableEntity::renderWork()
    {
        auto image_index = engine->renderer->getCurrentImage();
        updateUniformBuffer(image_index);

        if (engine->config->renderer.RasterizationEnabled())
        {
            auto [render_command, shadowmap_command] = getRenderCommand();
            engine->worker_pool->command_buffers.graphics_secondary.queue(*render_command);
            if (engine->config->renderer.RendererShadowmappingEnabled())
                engine->worker_pool->command_buffers.shadowmap.queue(*shadowmap_command);
            engine->worker_pool->gpuResource(std::move(render_command), std::move(shadowmap_command));
        }
        co_return;
    }

    void RenderableEntity::updateUniformBuffer(int image_index)
    {
        RenderableEntity::UniformBufferObject* ubo = reinterpret_cast<RenderableEntity::UniformBufferObject*>(uniform_buffer_mapped + (image_index * engine->renderer->uniform_buffer_align_up(sizeof(RenderableEntity::UniformBufferObject))));
        ubo->model_prev = model_prev;
        ubo->model = getModelMatrix();
        ubo->modelIT = glm::transpose(glm::inverse(glm::mat3(ubo->model)));
        //save the current model matrix for next frame's model_prev
        model_prev = ubo->model;
    }

    void RenderableEntity::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto& model = models[i];
            if (model->bottom_level_as)
            {
                //glm is column-major so we have to transpose the model matrix for Raytrace
                auto matrix = glm::mat3x4{ glm::transpose(getModelMatrix()) };
                engine->renderer->populateAccelerationStructure(as, model->bottom_level_as.get(), matrix, model->resource_index, static_cast<uint32_t>(RaytraceQueryer::ObjectFlags::DynamicEntities), 0);
            }
        }
    }

    glm::mat4 RenderableEntity::getModelMatrix()
    {
        return this->pos_mat * this->rot_mat * this->scale_mat;
    }

    glm::mat4 RenderableEntity::getPrevModelMatrix()
    {
        return this->model_prev;
    }

    WorkerTask<> RenderableEntity::InitWork()
    {
        //priority: 0
        auto initializer = std::make_unique<RenderableEntityInitializer>(this);
        engine->renderer->initEntity(initializer.get());
        engine->worker_pool->gpuResource(std::move(initializer));
        co_return;
    }

    WorkerTask<> RenderableEntity::InitModel(std::shared_ptr<Model> model, ModelTransformedGeometry& model_transform)
    {
        //priority: 0
        auto initializer = std::make_unique<RenderableEntityInitializer>(this);
        engine->renderer->initModel(initializer.get(), *model, model_transform);
        engine->worker_pool->gpuResource(std::move(initializer));
        co_return;
    }

    WorkerTask<> RenderableEntity::ReInitWork()
    {
        co_return;
    }

    std::pair<vk::UniqueCommandBuffer, vk::UniqueCommandBuffer> RenderableEntity::getRenderCommand()
    {
        auto initializer = std::make_unique<RenderableEntityInitializer>(this);

        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *engine->renderer->graphics_pool;
        alloc_info.commandBufferCount = 2;

        auto command_buffer = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        engine->renderer->drawEntity(initializer.get(), *command_buffer[0], engine->renderer->getCurrentImage());
        engine->renderer->drawEntityShadowmap(initializer.get(), *command_buffer[1], engine->renderer->getCurrentImage());

        engine->worker_pool->gpuResource(std::move(initializer));

        return { std::move(command_buffer[0]), std::move(command_buffer[1]) };
    }
}
