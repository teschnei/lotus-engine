#define M_PI 3.141592

const uint //SceneInfo
    eMeshInfo = 0,
    eTextures = 1
    ;

const uint //RTInfo
    eAS = 0
    ;

struct Mesh
{
    uint64_t vertex_buffer;
    uint64_t vertex_prev_buffer;
    uint64_t index_buffer;
    uint64_t material;
    vec3 scale;
    uint billboard;
    vec4 colour;
    vec2 uv_offset;
    float animation_frame;
    uint index_count;
    mat4x4 model_prev;
};

struct Material
{
    int texture_index;
    float specular_exponent;
    float specular_intensity;
    uint light_type;
};

struct Lights
{
    vec4 diffuse_color;
    vec4 specular_color;
    vec4 ambient_color;
    vec4 fog_color;
    float max_fog;
    float min_fog;
    float brightness;
    float _pad;
};

struct LightInfo
{
    vec3 pos;
    float intensity;
    vec3 colour;
    float radius;
    uint id;
    float _pad[3];
};

struct LightBuffer
{
    Lights entity;
    Lights landscape;
    vec3 diffuse_dir;
    uint light_count;
    float skybox_altitudes1;
    float skybox_altitudes2;
    float skybox_altitudes3;
    float skybox_altitudes4;
    float skybox_altitudes5;
    float skybox_altitudes6;
    float skybox_altitudes7;
    float skybox_altitudes8;
    vec4 skybox_colors[8];
};
