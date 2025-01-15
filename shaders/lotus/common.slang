implementing lotus;

public static const float M_PI = 3.14159265f;

namespace lotus
{
public struct Camera
{
    public float4x4 proj;
    public float4x4 view;
    public float4x4 proj_inverse;
    public float4x4 view_inverse;
    public float4 eye_pos;
};

public struct ModelUBO {
    public float4x4 model;
    public float4x4 model_IT;
    public float4x4 model_prev;
};

public struct Mesh
{
    public uint64_t vertex_buffer;
    public uint64_t vertex_prev_buffer;
    public uint64_t index_buffer;
    public uint64_t material;
    public float3 scale;
    public uint billboard;
    public float4 colour;
    public float2 uv_offset;
    public float animation_frame;
    public uint index_count;
    public float4x4 model_prev;
};

//TODO: verify if padding is needed in slang
public struct Material
{
    public int texture_index;
    public float2 roughness;
    public float ior;
    public uint light_type;
};

//TODO: remove me
public struct LightAmbient
{
    public float4 diffuse_color;
    public float4 specular_color;
    public float4 ambient_color;
    public float4 fog_color;
    public float max_fog;
    public float min_fog;
    public float brightness;
    float _pad;
};

public struct LightInfo
{
    public float3 pos;
    public float intensity;
    public float3 colour;
    public float radius;
    public uint id;
    float _pad[3];
};

public struct LightBuffer
{
    public LightAmbient entity;
    public LightAmbient landscape;
    public float3 diffuse_dir;
    public uint light_count;
    public float skybox_altitudes1;
    public float skybox_altitudes2;
    public float skybox_altitudes3;
    public float skybox_altitudes4;
    public float skybox_altitudes5;
    public float skybox_altitudes6;
    public float skybox_altitudes7;
    public float skybox_altitudes8;
    public float4 skybox_colors[8];
};

public struct Lights
{
    public LightBuffer light;
    //public LightInfo light_info[];
    //TODO
    public LightInfo light_info;
};
}
