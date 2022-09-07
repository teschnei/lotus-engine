#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"
#include "sampling.glsl"

struct Vertex
{
    vec3 pos;
    vec3 norm;
    vec4 color;
    vec2 uv;
};

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 0, set = 2) buffer readonly Vertices
{
    vec4 v[];
} vertices[];

layout(binding = 1, set = 2) buffer readonly Indices
{
    int i[];
} indices[];

layout(binding = 2, set = 2) uniform sampler2D textures[];

layout(binding = 3, set = 2) uniform MaterialInfo
{
    Material m;
} materials[];

layout(binding = 4, set = 2) buffer readonly MeshInfo
{
    Mesh m[];
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
    vec3 prev_pos;
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
    v.color = vec4(d1.z, d1.w, d2.x, d2.y);
    v.uv = vec2(d2.z, d2.w);
    return v;
}

void main()
{
    float distance = hitValue.distance + gl_HitTEXT;
    hitValue.distance = distance;
    if (distance > light.light.landscape.max_fog)
    {
        hitValue.diffuse = vec3(M_PI);
        hitValue.BRDF = light.light.landscape.fog_color.rgb / M_PI;
        hitValue.depth = 10;
        hitValue.prev_pos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_RayTmaxEXT;
        hitValue.origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_RayTmaxEXT;
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

    vec3 primitive_color = (v0.color * barycentrics.x + v1.color * barycentrics.y + v2.color * barycentrics.z).xyz;

    vec2 uv = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;
    uv += meshInfo.m[gl_InstanceCustomIndexEXT+gl_GeometryIndexEXT].uv_offset;
    uint material_index = meshInfo.m[gl_InstanceCustomIndexEXT+gl_GeometryIndexEXT].material_index;
    uint texture_index = materials[material_index].m.texture_index;
    vec3 texture_color = texture(textures[texture_index], uv).xyz;

    vec3 transformed_v0 = mat3(gl_ObjectToWorldEXT) * v0.pos;
    vec3 transformed_v1 = mat3(gl_ObjectToWorldEXT) * v1.pos;
    vec3 transformed_v2 = mat3(gl_ObjectToWorldEXT) * v2.pos;
    vec3 vertex_vec1 = normalize(vec3(transformed_v1 - transformed_v0));
    vec3 vertex_vec2 = normalize(vec3(transformed_v2 - transformed_v0));

    vec3 cross_vec = normalize(cross(vertex_vec1, vertex_vec2));

    if ((dot(cross_vec, normalized_normal)) < 0)
        cross_vec = -cross_vec;

    vec3 trace_origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + cross_vec * 0.001;
    vec3 diffuse = vec3(0);//light.light.landscape.ambient_color.rgb * light.light.landscape.brightness;

    for (int i = 0; i < light.light.light_count; i++)
    {
        vec3 diff = light.light_info[i].pos - trace_origin;
        vec3 dir = normalize(diff);
        float length = length(diff);
        float radius = light.light_info[i].radius;
        if (length > radius)
        {
            float att = 1;
            float intensity = light.light_info[i].intensity;
            int use_pdf = 1;
            if (intensity > 0)
            {
                att = max(1, radius * radius) / (length * length);
            }
            else
            {
                //for sun/moon, just skip pdf because I don't want to figure out the omegahuge intensity value for it to work
                intensity = 1;
                use_pdf = 0;
            }
            shadow.shadow = vec4(0.0);

            shadow.light = vec4(light.light_info[i].colour * intensity, 1.0);
            vec3 trace_dir = dir;
            float pdf = 1;

            if (radius > 0)
            {
                vec3 tangent, bitangent;
                createCoordinateSystem(dir, tangent, bitangent);

                float temp_pdf;
                trace_dir = samplingCone(hitValue.seed, temp_pdf, tangent, bitangent, dir, radius, dot(diff, diff));
                if (use_pdf > 0)
                    pdf = temp_pdf;
            }

            att *= dot(trace_dir, normalized_normal);
            if (att > 0.001)
            {
                traceRayEXT(topLevelAS, gl_RayFlagsSkipClosestHitShaderEXT, 0x01 | 0x02 | 0x10, 1, 0, 2, trace_origin, 0.000, trace_dir, length, 1);
                diffuse += att * shadow.shadow.rgb / pdf;
            }
        }
        else
        {
            diffuse += shadow.shadow.rgb;
        }
    }

    vec3 tangent, bitangent;
    float pdf;
    createCoordinateSystem(normalized_normal, tangent, bitangent);
    hitValue.direction = samplingHemisphere(hitValue.seed, pdf, tangent, bitangent, normalized_normal);
    hitValue.origin = trace_origin.xyz;
    hitValue.prev_pos = trace_origin.xyz;

    //const float p = cos_theta / M_PI;

    vec3 albedo = texture_color.rgb * primitive_color;

    if (distance > light.light.landscape.min_fog && distance < light.light.landscape.max_fog)
    {
        //albedo = mix(albedo, light.light.landscape.fog_color.rgb, (distance - light.light.landscape.min_fog) / (light.light.landscape.max_fog - light.light.landscape.min_fog));
        //diffuse = mix(diffuse, vec3(M_PI), (distance - light.light.landscape.min_fog) / (light.light.landscape.max_fog - light.light.landscape.min_fog));
    }

    vec3 BRDF = albedo / M_PI;
    hitValue.BRDF = BRDF;
    hitValue.diffuse = diffuse;
    //the weight is cos_theta / probability - for cosine weighted hemisphere, the cos_thetas cancel (will need updating for other sampling methods)
    //also, for lambertian, this M_PI cancels out the one in BRDF, but i'll leave it uncanceled for when more complex BRDFs arrive
    hitValue.weight = M_PI;
    hitValue.normal = normalized_normal;
}
