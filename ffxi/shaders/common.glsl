#define M_PI 3.141592

struct Mesh
{
    int vertex_offset;
    int index_offset;
    uint indices;
    int material_index;
    vec3 scale;
    uint billboard;
    vec4 colour;
    vec2 uv_offset;
    float animation_frame;
    uint vertex_prev_offset;
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
