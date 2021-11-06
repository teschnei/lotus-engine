#pragma once
#include "engine/entity/component/component.h"
#include "engine/light_manager.h"
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace FFXI
{
    class LandscapeComponent : public lotus::Component::Component<LandscapeComponent>
    {
    public:
        struct LightTOD
        {
            glm::vec4 sunlight_diffuse_entity;
            glm::vec4 moonlight_diffuse_entity;
            glm::vec4 ambient_entity;
            glm::vec4 fog_color_entity;
            float max_fog_entity;
            float min_fog_entity;
            float brightness_entity;
            glm::vec4 sunlight_diffuse_landscape;
            glm::vec4 moonlight_diffuse_landscape;
            glm::vec4 ambient_landscape;
            glm::vec4 fog_color_landscape;
            float max_fog_landscape;
            float min_fog_landscape;
            float brightness_landscape;
            float skybox_altitudes[8];
            glm::vec4 skybox_colors[8];
        };
        explicit LandscapeComponent(lotus::Entity*, lotus::Engine* engine, std::map<std::string, std::map<uint32_t, LightTOD>>&& weather_map);

        lotus::Task<> tick(lotus::time_point time, lotus::duration delta);

    protected:

        std::map<std::string, std::map<uint32_t, LightTOD>> weather_light_map;
        std::string current_weather = "suny";
        float current_time{750};
        lotus::LightID light_id{ 0 };
    };
}
