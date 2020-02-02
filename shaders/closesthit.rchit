#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct Vertex
{
    vec3 pos;
    vec3 norm;
    vec2 uv;
};

layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;
layout(binding = 1, set = 0) buffer Vertices
{
    Vertex v[];
} vertices[1024];

layout(binding = 2, set = 0) buffer Indices
{
    int i[];
} indices[1024];

layout(binding = 3, set = 0) uniform sampler2D textures[1024];

struct Mesh
{
    int vec_index_offset;
    int tex_offset;
    float specular_exponent;
    float specular_intensity;
};

layout(binding = 4, set = 0) uniform MeshInfo
{
    Mesh m[1024];
} meshInfo;

layout(binding = 2, set = 1) uniform Light
{
    vec3 light;
    float pad;
    vec3 color;
} light;

layout(location = 0) rayPayloadInNV HitValue
{
    vec3 color;
} hitValue;

layout(location = 1) rayPayloadNV bool shadow;

layout(shaderRecordNV) buffer Block
{
    uint geometry_index;
} block;

hitAttributeNV vec3 attribs;

ivec3 getIndex(uint primitive_id)
{
    uint resource_index = meshInfo.m[gl_InstanceCustomIndexNV+block.geometry_index].vec_index_offset;
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
    uint resource_index = meshInfo.m[gl_InstanceCustomIndexNV+block.geometry_index].vec_index_offset;
    Vertex v = vertices[resource_index].v[index];

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

    float dot_product = dot(-light.light, normalized_normal);

    vec2 uv = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;
    Mesh mesh = meshInfo.m[gl_InstanceCustomIndexNV+block.geometry_index];
    vec4 color = texture(textures[mesh.tex_offset], uv);

    shadow = true;
    if (dot_product > 0)
    {
        vec3 vertex_vec1 = normalize(vec3(v1.pos - v0.pos));
        vec3 vertex_vec2 = normalize(vec3(v2.pos - v0.pos));

        vec3 cross_vec = normalize(cross(vertex_vec1, vertex_vec2));

        if ((dot(cross_vec, normalized_normal)) < 0)
            cross_vec = -cross_vec;

        vec3 origin = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV + cross_vec * 0.001;
        traceNV(topLevelAS, gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsSkipClosestHitShaderNV, 0xFF, 32, 1, 1, origin, 0.000, -light.light, 500, 1);
    }
    vec3 ambient = vec3(0.5, 0.5, 0.5); //ambient
    vec3 specular = vec3(0);
    vec3 diffuse = vec3(0);
    if (!shadow)
    {
        vec3 ray = normalize(gl_WorldRayDirectionNV);
        vec3 reflection = normalize(reflect(-light.light, normalized_normal));
        float specular_dot = dot(ray, reflection);
        float specular_factor = mesh.specular_intensity * color.a;
        if (specular_dot > 0)
        {
            specular_dot = pow(specular_dot, mesh.specular_exponent);
            specular = light.color.rgb * specular_factor * specular_dot;
        }
        diffuse = light.color.rgb * max(dot_product, 0.0);
    }
    vec3 total_light = ambient + specular + diffuse;

    hitValue.color = color.rgb * total_light;
}
