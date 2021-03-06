#pragma once

#include <memory>
#include "renderable_entity.h"

namespace lotus
{
    class Particle : public RenderableEntity
    {
    public:
        Particle(Engine*, duration lifetime, std::shared_ptr<Model> model);
        static Task<std::shared_ptr<Particle>> Init(Engine* engine, duration lifetime, std::shared_ptr<Model> model);
        virtual ~Particle() = default;

        class Billboard
        {
        public:
            static constexpr uint8_t None = 0;
            static constexpr uint8_t X = 1;
            static constexpr uint8_t Y = 2;
            static constexpr uint8_t Z = 4;
            static constexpr uint8_t All = 7;
        };

        uint8_t billboard{ 0 };

        glm::mat4 getModelMatrix();

        glm::vec4 color{ 1.f };

        duration getLifetime() { return lifetime; }
        time_point getSpawnTime() { return spawn_time; }

        virtual void populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index) override;
        virtual WorkerTask<> ReInitWork() override;

        uint64_t resource_index{ 0 };

        glm::vec3 base_pos{};
        glm::vec3 base_rot{};
        glm::vec3 base_scale{};

    protected:
        WorkerTask<> Load();
        WorkerTask<> InitWork();
        virtual Task<> tick(time_point time, duration delta) override;
        virtual Task<> render(Engine* engine, std::shared_ptr<Entity> sp) override;
        WorkerTask<> renderWork();
        friend class ParticleParentComponent;
        glm::mat4 offset_mat{1.f};

        duration lifetime;
        time_point spawn_time;
    };
}
