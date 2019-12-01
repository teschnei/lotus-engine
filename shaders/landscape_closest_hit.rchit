#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct Vertex
{
    vec3 pos;
    vec3 norm;
    vec3 color;
    vec2 uv;
    float _pad;
};

layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;
layout(binding = 1, set = 0) buffer Vertices
{
    vec4 v[];
} vertices[1024];

layout(binding = 2, set = 0) buffer Indices
{
    int i[];
} indices[1024];

layout(binding = 3, set = 0) uniform sampler2D textures[1024];

layout(binding = 2, set = 1) uniform Light
{
    vec3 light;
    float pad;
    vec3 color;
} light;

layout(location = 0) rayPayloadInNV HitValue
{
    vec3 color;
    uint max_index;
    float min_dist;
} hitValue;

layout(location = 1) rayPayloadNV bool shadow;

layout(shaderRecordNV) buffer Block
{
    uint geometry_index;
} block;

hitAttributeNV vec3 attribs;

ivec3 getIndex(uint primitive_id)
{
    uint resource_index = gl_InstanceCustomIndexNV+block.geometry_index;
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
    uint resource_index = gl_InstanceCustomIndexNV+block.geometry_index;
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
    ivec3 primitive_indices = getIndex(gl_PrimitiveID);
    Vertex v0 = unpackVertex(primitive_indices.x);
    Vertex v1 = unpackVertex(primitive_indices.y);
    Vertex v2 = unpackVertex(primitive_indices.z);

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    vec3 normal = v0.norm * barycentrics.x + v1.norm * barycentrics.y + v2.norm * barycentrics.z;
    vec3 transformed_normal = mat3(gl_ObjectToWorldNV) * normal;
    vec3 normalized_normal = normalize(transformed_normal);

    float dot_product = max(dot(-light.light, normalized_normal), 0.5);

    vec3 color = dot_product * (v0.color * barycentrics.x + v1.color * barycentrics.y + v2.color * barycentrics.z);

    vec2 uv = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;
    uint resource_index = gl_InstanceCustomIndexNV+block.geometry_index;
    color *= texture(textures[resource_index], uv).xyz;

    vec3 vertex_vec1 = normalize(vec3(v1.pos - v0.pos));
    vec3 vertex_vec2 = normalize(vec3(v2.pos - v0.pos));

    vec3 cross_vec = normalize(cross(vertex_vec1, vertex_vec2));

    if ((dot(cross_vec, normalized_normal)) < 0)
        cross_vec = -cross_vec;

    vec3 origin = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV + cross_vec * 0.001;
    shadow = true;
    traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 16, 1, 1, origin, 0.000, -light.light, 500, 1);
    vec3 ambient = vec3(0.5, 0.5, 0.5); //ambient
    vec3 total_light = vec3(0);
    if (!shadow)
        total_light = light.color;
    total_light = max(ambient, total_light);

    hitValue.color = color * total_light;
}
