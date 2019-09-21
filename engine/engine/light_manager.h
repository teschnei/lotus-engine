#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "renderer/memory.h"

namespace lotus
{
    class Engine;

    struct PointLight
    {
        glm::vec3 pos;
        float min_range; // radial offset of light source (so that they can be positioned inside things)
        float max_range; // aka light intensity
    };

    struct DirectionalLight
    {
        glm::vec3 direction;
        float _pad;
        glm::vec3 color;
        float _pad2;
    };

    class LightManager
    {
    public:
        explicit LightManager(Engine* engine);

        void UpdateLightBuffer();

        DirectionalLight directional_light {};
        std::vector<PointLight> point_lights;

        std::unique_ptr<Buffer> dir_buffer;

    private:
        Engine* engine;
    };
}
