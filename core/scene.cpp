#include "scene.h"
#include "entity/renderable_entity.h"
#include "core.h"
#include "renderer/renderer.h"

namespace lotus
{
void Scene::render(Engine* engine)
{
    for (const auto& entity : entities)
    {
        if (auto renderable_entity = std::dynamic_pointer_cast<RenderableEntity>(entity))
        {
            renderable_entity->render(engine, renderable_entity);
        }
    }
}
}

