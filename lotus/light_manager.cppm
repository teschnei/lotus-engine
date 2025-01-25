module;

#include <memory>
#include <shared_mutex>
#include <vector>

export module lotus:core.light_manager;

import :renderer.memory;
import glm;

export namespace lotus
{
class Engine;

using LightID = uint32_t;

struct Light
{
    glm::vec3 pos;
    float intensity;
    glm::vec3 colour;
    float radius;
    LightID id;
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
};

struct LightBuffer
{
    Lights entity;
    Lights landscape;
    glm::vec3 diffuse_dir;
    uint32_t light_count;
    float skybox_altitudes1;
    float skybox_altitudes2;
    float skybox_altitudes3;
    float skybox_altitudes4;
    float skybox_altitudes5;
    float skybox_altitudes6;
    float skybox_altitudes7;
    float skybox_altitudes8;
    glm::vec4 skybox_colors[8];
};

class LightManager
{
public:
    explicit LightManager(Engine* engine);
    ~LightManager();

    void UpdateLightBuffer();
    size_t GetBufferSize();
    LightID AddLight(Light light);
    void RemoveLight(LightID);
    void UpdateLight(LightID, Light);

    LightBuffer light{};
    std::vector<Light> lights;

    std::unique_ptr<Buffer> light_buffer;
    size_t lights_buffer_count{0};

private:
    Engine* engine;
    uint8_t* light_buffer_map{nullptr};
    std::shared_mutex light_buffer_mutex;
    size_t cur_light_id{0};
};
} // namespace lotus
