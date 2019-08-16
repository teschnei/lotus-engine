#pragma once
#include <memory>
#include "types.h"
#include "scene.h"
#include "core.h"

namespace lotus
{
    class Game
    {
    public:
        Game(const std::string& appname, uint32_t appversion) : engine(std::make_unique<Engine>(appname, appversion, this)) {}
        virtual ~Game() = default;

        virtual void run() { engine->run(); }
        virtual void tick(time_point time, duration delta) = 0;

        std::unique_ptr<Scene> scene;
        std::unique_ptr<Engine> engine;

        glm::mat4 cascade_matrices[Renderer::shadowmap_cascades];
        std::unique_ptr<Buffer> cascade_matrices_ubo;

        struct UBOFS
        {
            glm::vec4 cascade_splits;
            glm::mat4 cascade_view_proj[Renderer::shadowmap_cascades];
            glm::mat4 inverse_view;
            glm::vec3 light_dir;
            float _pad;
        } cascade_data;

        std::unique_ptr<Buffer> cascade_data_ubo;
    };
}
