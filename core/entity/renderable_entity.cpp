#include "renderable_entity.h"
#include "core.h"
#include "task/entity_render.h"

namespace lotus
{
    RenderableEntity::RenderableEntity() : Entity()
    {
    }

    void RenderableEntity::setScale(float x, float y, float z)
    {
        this->scale = glm::vec3(x, y, z);
        this->scale_mat = glm::scale(glm::mat4{ 1.f }, glm::vec3{ x, y, z });
    }

    void RenderableEntity::render(Engine* engine, std::shared_ptr<RenderableEntity>& sp)
    {
        engine->worker_pool.addWork(std::make_unique<lotus::EntityRenderTask>(sp));
    }

    glm::mat4 RenderableEntity::getModelMatrix()
    {
        return this->pos_mat * this->rot_mat * this->scale_mat;
    }
}
