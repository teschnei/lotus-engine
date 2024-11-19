#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "common.glsl"

struct Vertex
{
    vec3 pos;
    float _pad;
    vec3 norm;
    float _pad2;
    vec4 colour;
    vec2 uv;
    vec2 _pad3;
};

layout(set = 0, binding = eAS) uniform accelerationStructureEXT topLevelAS;

layout(set = 1, binding = 6, std430) buffer readonly Light
{
    LightBuffer light;
    LightInfo light_info[];
} light;

layout(set = 2, binding = eMeshInfo) buffer readonly MeshInfo
{
    Mesh m[];
} meshInfo;
layout(set = 2, binding = eTextures) uniform sampler2D textures[];

layout(buffer_reference, scalar) buffer Indices { uint16_t i[]; };
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Materials { Material m; };

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
    vec3 prev_pos;
} hitValue;

layout(location = 1) rayPayloadEXT Shadow
{
    vec4 light;
    vec4 shadow;
} shadow;

hitAttributeEXT vec2 attribs;

void main()
{
    if (gl_HitTEXT > light.light.entity.max_fog)
    {
        hitValue.depth = 10;
        hitValue.diffuse = vec3(M_PI);
        hitValue.BRDF = light.light.entity.fog_color.rgb / M_PI;
        hitValue.distance = gl_HitTEXT;
        return;
    }
    hitValue.depth++;

    Mesh mesh = meshInfo.m[gl_InstanceCustomIndexEXT+gl_GeometryIndexEXT];

    Indices indices = Indices(mesh.index_buffer);
    uvec3 primitive_indices = uvec3(
        uint(indices.i[gl_PrimitiveID * 3 + 0]),
        uint(indices.i[gl_PrimitiveID * 3 + 1]),
        uint(indices.i[gl_PrimitiveID * 3 + 2])
    );

    Vertices vertices = Vertices(mesh.vertex_buffer);
    Vertex v0 = vertices.v[primitive_indices.x];
    Vertex v1 = vertices.v[primitive_indices.y];
    Vertex v2 = vertices.v[primitive_indices.z];
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    vec3 normal = v0.norm * barycentrics.x + v1.norm * barycentrics.y + v2.norm * barycentrics.z;
    vec3 transformed_normal = mat3(gl_ObjectToWorldEXT) * normal;
    vec3 normalized_normal = normalize(transformed_normal);

    float dot_product = dot(-light.light.diffuse_dir, normalized_normal);

    vec2 uv = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;
    uv += mesh.uv_offset;
    Materials materials = Materials(mesh.material);
    uint texture_index = materials.m.texture_index;
    vec4 texture_colour = texture(textures[nonuniformEXT(texture_index)], uv);
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
    uint flags = 0x01 | 0x02 | 0x20;
    if (hitValue.depth < 3)
        flags |= 0x10;
    traceRayEXT(topLevelAS, 0, flags, 0, 0, 0, origin.xyz, 0.0, gl_WorldRayDirectionEXT, 1000.0 - gl_RayTmaxEXT, 0);
    hitValue.particle += colour;
}
