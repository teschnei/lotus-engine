#pragma once
#include <filesystem>
#include "engine/entity/landscape_entity.h"
#include "engine/light_manager.h"
#include "engine/renderer/mesh.h"
#include "dat/mzb.h"

class FFXILandscapeEntity : public lotus::LandscapeEntity
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
    FFXILandscapeEntity(lotus::Engine* engine) : LandscapeEntity(engine) {}
    static lotus::Task<std::shared_ptr<FFXILandscapeEntity>> Init(lotus::Engine* engine, const std::filesystem::path& dat);
    virtual void populate_AS(lotus::TopLevelAccelerationStructure* as, uint32_t image_index) override;
    FFXI::QuadTree quadtree{glm::vec3{}, glm::vec3{}};
    std::vector<std::pair<uint32_t, InstanceInfo>> model_vec;
    std::map<std::string, std::map<uint32_t, LightTOD>> weather_light_map;
protected:
    virtual lotus::Task<> render(lotus::Engine* engine, std::shared_ptr<Entity> sp) override;
    virtual lotus::Task<> tick(lotus::time_point time, lotus::duration delta) override;
    lotus::WorkerTask<> Load(const std::filesystem::path& dat);
    float current_time{750};
    std::string current_weather = "suny";
    lotus::LightID sunlight{ 0 };
    lotus::LightID moonlight{ 0 };
};

class CollisionMesh : public lotus::Mesh
{
public:
    CollisionMesh() : Mesh() {}

    std::unique_ptr<lotus::Buffer> transform_buffer;
};
