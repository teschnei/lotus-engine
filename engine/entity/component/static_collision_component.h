#pragma once
#include "component.h"
#include <memory>
#include <vector>
#include "engine/worker_task.h"
#include "engine/renderer/model.h"

namespace lotus
{
    class CollisionMesh : public lotus::Mesh
    {
    public:
        CollisionMesh() : Mesh() {}

        std::unique_ptr<lotus::Buffer> transform_buffer;
    };

    namespace Component
    {
        class StaticCollisionComponent : public Component<StaticCollisionComponent>
        {
        public:

            explicit StaticCollisionComponent(Entity*, Engine* engine, std::vector<std::shared_ptr<Model>> models);
                
            Task<> tick(time_point time, duration delta);

        protected:
            std::vector<std::shared_ptr<Model>> models;
        };
    }
}
