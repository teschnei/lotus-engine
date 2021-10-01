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
    vec4 colour;
    vec2 uv;
};

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0) buffer readonly Vertices
{
    vec4 v[];
} vertices[1024];

layout(binding = 3, set = 0) buffer readonly Indices
{
    int i[];
} indices[1024];

layout(binding = 4, set = 0) uniform sampler2D textures[1024];

layout(binding = 5, set = 0) uniform MaterialInfo
{
    Material m;
} materials[1024];

layout(binding = 6, set = 0) buffer readonly MeshInfo
{
    Mesh m[1024];
} meshInfo;

layout(std430, binding = 6, set = 1) buffer readonly Light
{
    LightBuffer light;
    LightInfo light_info[];
} light;

layout(location = 0) rayPayloadInEXT HitValue
{
    vec3 BRDF;
    vec3 diffuse;
    vec3 normal;
    int depth;
    uint seed;
    float weight;
    vec3 origin;
    vec3 direction;
    float distance;
    vec3 particle;
} hitValue;

layout(location = 1) rayPayloadEXT Shadow 
{
    vec4 light;
    vec4 shadow;
} shadow;

hitAttributeEXT block {
    vec2 bary_coord;
    uint primitive_id;
} attribs;

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
    v.colour = vec4(d1.z, d1.w, d2.x, d2.y);
    v.uv = vec2(d2.z, d2.w);
    return v;
}

void main()
{
    if (gl_HitTEXT > light.light.entity.max_fog)
    {
        hitValue.depth = 10;
        hitValue.BRDF = vec3(1.0);
        hitValue.diffuse = light.light.entity.fog_color.rgb;
        hitValue.distance = gl_HitTEXT;
        return;
    }
    ivec3 primitive_indices = getIndex(attribs.primitive_id);
    Vertex v0 = unpackVertex(primitive_indices.x);
    Vertex v1 = unpackVertex(primitive_indices.y);
    Vertex v2 = unpackVertex(primitive_indices.z);

    const vec3 barycentrics = vec3(1.0 - attribs.bary_coord.x - attribs.bary_coord.y, attribs.bary_coord.x, attribs.bary_coord.y);

    vec3 normal = v0.norm * barycentrics.x + v1.norm * barycentrics.y + v2.norm * barycentrics.z;
    vec3 transformed_normal = mat3(gl_ObjectToWorldEXT) * normal;
    vec3 normalized_normal = normalize(transformed_normal);

    float dot_product = dot(-light.light.diffuse_dir, normalized_normal);

    vec2 uv = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;
    Mesh mesh = meshInfo.m[gl_InstanceCustomIndexEXT+gl_GeometryIndexEXT];
    uv += mesh.uv_offset;
    vec4 texture_colour = texture(textures[mesh.material_index], uv);
    vec4 model_colour = v0.colour * barycentrics.x + v1.colour * barycentrics.y + v2.colour * barycentrics.z;
    vec3 colour = model_colour.rgb * mesh.colour.rgb * texture_colour.rgb * 4;
    float a = model_colour.a * mesh.colour.a * texture_colour.a * 2;
    colour *= a;

    vec3 transformed_v0 = mat3(gl_ObjectToWorldEXT) * v0.pos;
    vec3 transformed_v1 = mat3(gl_ObjectToWorldEXT) * v1.pos;
    vec3 transformed_v2 = mat3(gl_ObjectToWorldEXT) * v2.pos;
    vec3 vertex_vec1 = normalize(vec3(transformed_v1 - transformed_v0));
    vec3 vertex_vec2 = normalize(vec3(transformed_v2 - transformed_v0));

    vec3 cross_vec = normalize(cross(vertex_vec1, vertex_vec2));

    if ((dot(cross_vec, gl_WorldRayDirectionEXT)) < 0)
        cross_vec = -cross_vec;

    vec3 origin = gl_WorldRayOriginEXT + (gl_WorldRayDirectionEXT * gl_RayTmaxEXT) + cross_vec * 0.001;
    traceRayEXT(topLevelAS, 0, 0x01 | 0x02 | 0x10, 0, 0, 0, origin.xyz, 0.0, gl_WorldRayDirectionEXT, 1000.0 - gl_RayTmaxEXT, 0);
    hitValue.particle += colour;
}
