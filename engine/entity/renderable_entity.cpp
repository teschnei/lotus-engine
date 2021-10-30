#include "renderable_entity.h"
#include "engine/core.h"
#include "component/animation_component.h"

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
        co_return;
    }

    WorkerTask<> RenderableEntity::renderWork()
    {
        co_return;
    }

    void RenderableEntity::updateUniformBuffer(int image_index)
    {
    }

    void RenderableEntity::populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index)
    {
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
        co_return;
    }

    WorkerTask<> RenderableEntity::InitModel(std::shared_ptr<Model> model, ModelTransformedGeometry& model_transform)
    {
        co_return;
    }

    WorkerTask<> RenderableEntity::ReInitWork()
    {
        co_return;
    }
}
