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
layout(set = 2, binding = eTextures) uniform sampler2DArray textures[];

layout(buffer_reference, scalar) buffer Indices { uint16_t i[]; };
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Materials { Material m; };

struct HitValue
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
};

layout(location = 0) rayPayloadInEXT HitValue hitValue;

layout(location = 1) rayPayloadEXT Shadow
{
    vec4 light;
    vec4 shadow;
} shadow;

hitAttributeEXT vec2 attribs;

float fresnel(vec3 ray, vec3 normal, float ior_in, float ior_out)
{
    float reflect = 1.0;
    float cos_i = clamp(-1, 1, dot(ray, normal));

    float sin_t = (ior_in / ior_out) * sqrt(max(0.f, 1 - cos_i * cos_i));

    if (sin_t < 1)
    {
        float cos_t = sqrt(max(0.f, 1 - sin_t * sin_t));
        cos_i = abs(cos_i);
        float rs = ((ior_out * cos_i) - (ior_in * cos_t)) / ((ior_out * cos_i) + (ior_in * cos_t));
        float rp = ((ior_in * cos_i) - (ior_out * cos_t)) / ((ior_in * cos_i) + (ior_out * cos_t));
        reflect = (rs * rs + rp * rp) / 2;
    }
    return reflect;
}


void main()
{
    float distance = hitValue.distance + gl_HitTEXT;
    hitValue.distance = distance;
    if (distance > light.light.entity.max_fog)
    {
        hitValue.depth = 10;
        hitValue.BRDF = vec3(1.0);
        hitValue.diffuse = light.light.entity.fog_color.rgb;
        return;
    }
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

    vec2 uv = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;

    float prev_frame = floor(mesh.animation_frame);
    float next_frame = mod(floor(mesh.animation_frame+1), 30.f);
    Materials materials = Materials(mesh.material);
    uint texture_index = materials.m.texture_index;
    vec3 prev_normal = texture(textures[nonuniformEXT(texture_index)], vec3(uv, prev_frame)).xyz;
    vec3 next_normal = texture(textures[nonuniformEXT(texture_index)], vec3(uv, next_frame)).xyz;
    vec3 normal_map = mix(prev_normal, next_normal, mesh.animation_frame - prev_frame) * 2.0 - 1.0;

    vec3 normal = v0.norm * barycentrics.x + v1.norm * barycentrics.y + v2.norm * barycentrics.z;
    vec3 transformed_normal = mat3(gl_ObjectToWorldEXT) * normal;
    vec3 normalized_normal = normalize(transformed_normal);

    //TODO: calculate these before
    vec3 t = vec3(1.0,0.0,0.0);
    vec3 b = vec3(0.0,0.0,1.0);
    mat3 tbn = mat3(t, b, normalized_normal);
    normalized_normal = normalize(tbn * normal_map);

    float ray_normal_dot = dot(normalized_normal, gl_WorldRayDirectionEXT);

    float ior_in = 1;
    float ior_out = 1.3;

    vec3 refract_normal = normalized_normal;

    if (ray_normal_dot < 0)
    {
        //entering
        ray_normal_dot = -ray_normal_dot;
    }
    else
    {
        //exiting
        ior_in = 1.3;
        ior_out = 1;
        refract_normal = -normalized_normal;
    }
    float reflect_weight = fresnel(gl_WorldRayDirectionEXT, normalized_normal, ior_in, ior_out);
    float refract_weight = 1.0 - reflect_weight;
    vec3 refraction = gl_WorldRayDirectionEXT;

    if (reflect_weight < 1.0)
    {
        float index_factor = ior_in / ior_out;
        float k = 1 - index_factor * index_factor * (1 - ray_normal_dot * ray_normal_dot);
        if (k > 0)
        {
            refraction = index_factor * gl_WorldRayDirectionEXT + (index_factor * ray_normal_dot - sqrt(k)) * refract_normal;
        }
    }

    vec3 transformed_v0 = mat3(gl_ObjectToWorldEXT) * v0.pos;
    vec3 transformed_v1 = mat3(gl_ObjectToWorldEXT) * v1.pos;
    vec3 transformed_v2 = mat3(gl_ObjectToWorldEXT) * v2.pos;
    vec3 vertex_vec1 = normalize(vec3(transformed_v1 - transformed_v0));
    vec3 vertex_vec2 = normalize(vec3(transformed_v2 - transformed_v0));

    vec3 cross_vec = normalize(cross(vertex_vec1, vertex_vec2));

    if ((dot(cross_vec, gl_WorldRayDirectionEXT)) < 0)
    {
        cross_vec = -cross_vec;
    }

    HitValue orig_hit = hitValue;
    vec3 BRDF = vec3(0.0);
    vec3 diffuse = vec3(0.0);
    vec3 new_normal = vec3(0.0);
    float weight = 0.0;

    vec3 origin = gl_WorldRayOriginEXT + (gl_WorldRayDirectionEXT * gl_RayTmaxEXT);
    vec3 offset = cross_vec * 0.001;
    if (refract_weight > 0.0)
    {
        traceRayEXT(topLevelAS, 0, 0x01 | 0x02 | 0x10, 0, 0, 0, origin + offset, 0.0, refraction, 1000.0 - distance, 0);
        BRDF += hitValue.BRDF * hitValue.diffuse * refract_weight;
        diffuse += hitValue.diffuse * refract_weight;
        new_normal += hitValue.normal * refract_weight;
        weight += hitValue.weight * refract_weight;
        hitValue = orig_hit;
    }
    if (reflect_weight > 0.0)
    {
        traceRayEXT(topLevelAS, 0, 0x01 | 0x02 | 0x10, 0, 0, 0, origin - offset, 0.0, reflect(gl_WorldRayDirectionEXT, -normalized_normal), 1000.0 - distance, 0);
        BRDF += hitValue.BRDF * hitValue.diffuse * reflect_weight;
        diffuse += hitValue.diffuse * reflect_weight;
        new_normal += hitValue.normal * reflect_weight;
        weight += hitValue.weight * reflect_weight;
        hitValue = orig_hit;
    }
    hitValue.BRDF = BRDF / diffuse;
    hitValue.diffuse = diffuse;
    hitValue.normal = new_normal;
    hitValue.weight = weight;
}
