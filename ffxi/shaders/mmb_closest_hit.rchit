#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

struct Vertex
{
    vec3 pos;
    vec3 norm;
    vec3 color;
    vec2 uv;
    float _pad;
};

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0) buffer Vertices
{
    vec4 v[];
} vertices[1024];

layout(binding = 2, set = 0) buffer Indices
{
    int i[];
} indices[1024];

layout(binding = 3, set = 0) uniform sampler2D textures[1024];

layout(binding = 4, set = 0) uniform MaterialInfo
{
    Material m;
} materials[1024];

layout(binding = 5, set = 0) uniform MeshInfo
{
    Mesh m[1024];
} meshInfo;

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

layout(std430, binding = 3, set = 1) uniform Light
{
    Lights entity;
    Lights landscape;
    vec3 diffuse_dir;
    float _pad;
    float skybox_altitudes1;
    float skybox_altitudes2;
    float skybox_altitudes3;
    float skybox_altitudes4;
    float skybox_altitudes5;
    float skybox_altitudes6;
    float skybox_altitudes7;
    float skybox_altitudes8;
    vec4 skybox_colors[8];
} light;

layout(location = 0) rayPayloadInEXT HitValue
{
    vec3 albedo;
    vec3 light;
} hitValue;

layout(location = 1) rayPayloadEXT Shadow 
{
    vec4 light;
    vec4 shadow;
} shadow;

hitAttributeEXT vec3 attribs;

ivec3 getIndex(uint primitive_id)
{
    uint resource_index = meshInfo.m[gl_InstanceCustomIndexEXT+gl_GeometryIndexEXT].index_offset;
    ivec3 ret;
    uint base_index = primitive_id * 3;
    if (base_index % 2 == 0)
    {
        ret.x = indices[resource_index].i[base_index / 2] & 0xFFFF;
        ret.y = (indices[resource_index].i[base_index / 2] >> 16) & 0xFFFF;
        ret.z = indices[resource_index].i[(base_index / 2) + 1] & 0xFFFF;
    }
    else
    {
        ret.x = (indices[resource_index].i[base_index / 2] >> 16) & 0xFFFF;
        ret.y = indices[resource_index].i[(base_index / 2) + 1] & 0xFFFF;
        ret.z = (indices[resource_index].i[(base_index / 2) + 1] >> 16) & 0xFFFF;
    }
    return ret;
}

uint vertexSize = 3;

Vertex unpackVertex(uint index)
{
    uint resource_index = meshInfo.m[gl_InstanceCustomIndexEXT+gl_GeometryIndexEXT].vertex_offset;
    Vertex v;

    vec4 d0 = vertices[resource_index].v[vertexSize * index + 0];
    vec4 d1 = vertices[resource_index].v[vertexSize * index + 1];
    vec4 d2 = vertices[resource_index].v[vertexSize * index + 2];

    v.pos = d0.xyz;
    v.norm = vec3(d0.w, d1.x, d1.y);
    v.color = vec3(d1.z, d1.w, d2.x);
    v.uv = vec2(d2.y, d2.z);
    return v;
}

void main()
{
    if (gl_HitTEXT > light.landscape.max_fog)
    {
        hitValue.albedo = light.landscape.fog_color.rgb;
        hitValue.light = vec3(1.0);
        return;
    }
    ivec3 primitive_indices = getIndex(gl_PrimitiveID);
    Vertex v0 = unpackVertex(primitive_indices.x);
    Vertex v1 = unpackVertex(primitive_indices.y);
    Vertex v2 = unpackVertex(primitive_indices.z);

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    vec3 normal = v0.norm * barycentrics.x + v1.norm * barycentrics.y + v2.norm * barycentrics.z;
    vec3 transformed_normal = mat3(gl_ObjectToWorldEXT) * normal;
    vec3 normalized_normal = normalize(transformed_normal);

    float dot_product = dot(-light.diffuse_dir, normalized_normal);

    vec3 primitive_color = (v0.color * barycentrics.x + v1.color * barycentrics.y + v2.color * barycentrics.z);

    vec2 uv = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;
    uint resource_index = meshInfo.m[gl_InstanceCustomIndexEXT+gl_GeometryIndexEXT].material_index;
    vec3 texture_color = texture(textures[resource_index], uv).xyz;

    shadow.light = vec4(light.landscape.diffuse_color.rgb * light.landscape.brightness, 1.0);
    shadow.shadow = vec4(0.0);
    if (dot_product > 0)
    {
        vec3 transformed_v0 = mat3(gl_ObjectToWorldEXT) * v0.pos;
        vec3 transformed_v1 = mat3(gl_ObjectToWorldEXT) * v1.pos;
        vec3 transformed_v2 = mat3(gl_ObjectToWorldEXT) * v2.pos;
        vec3 vertex_vec1 = normalize(vec3(transformed_v1 - transformed_v0));
        vec3 vertex_vec2 = normalize(vec3(transformed_v2 - transformed_v0));

        vec3 cross_vec = normalize(cross(vertex_vec1, vertex_vec2));

        if ((dot(cross_vec, normalized_normal)) < 0)
            cross_vec = -cross_vec;

        vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + cross_vec * 0.001;
        traceRayEXT(topLevelAS, gl_RayFlagsSkipClosestHitShaderEXT, 0x01 | 0x02 | 0x10 , 1, 0, 1, origin, 0.000, -light.diffuse_dir, 500, 1);
    }
    vec3 ambient = light.landscape.ambient_color.rgb;
    vec3 diffuse = vec3(0);
    diffuse = vec3(max(dot_product, 0.0)) * shadow.shadow.rgb;

    vec3 out_light = diffuse + ambient;
    vec3 out_color = primitive_color * texture_color;
    //todo: split these
    out_color = out_light * out_color;
    if (gl_HitTEXT > light.landscape.min_fog)
    {
        out_color = mix(out_color, light.landscape.fog_color.rgb, (gl_HitTEXT - light.landscape.min_fog) / (light.landscape.max_fog - light.landscape.min_fog));
    }
    hitValue.albedo = out_color;
    hitValue.light = vec3(1.0);
}
