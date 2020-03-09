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

    struct Lights
    {
        glm::vec4 diffuse_color;
        glm::vec4 specular_color;
        glm::vec4 ambient_color;
        glm::vec4 fog_color;
        float max_fog;
        float min_fog;
        float brightness;
        float _pad;
    };

    struct LightBuffer
    {
        Lights entity;
        Lights landscape;
        glm::vec3 diffuse_dir;
        float _pad;
        float skybox_altitudes[8];
        glm::vec4 skybox_colors[8];
    };

    class LightManager
    {
    public:
        explicit LightManager(Engine* engine);
        ~LightManager();

        void UpdateLightBuffer();

        LightBuffer light {};
        std::vector<PointLight> point_lights;

        std::unique_ptr<Buffer> light_buffer;

        LightBuffer* buffer_map{ nullptr };

    private:
        Engine* engine;
    };
}
