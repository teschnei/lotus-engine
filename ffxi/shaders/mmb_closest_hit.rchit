#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "common.glsl"
#include "sampling.glsl"

struct Vertex
{
    vec3 pos;
    float _pad;
    vec3 norm;
    float _pad2;
    vec4 color;
    vec2 uv;
    vec2 _pad3;
};

layout(set = 0, binding = eAS) uniform accelerationStructureEXT topLevelAS;

layout(std430, set = 1, binding = 6) buffer readonly Light
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

    vec3 primitive_color = (v0.color * barycentrics.x + v1.color * barycentrics.y + v2.color * barycentrics.z).xyz;

    vec2 uv = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;
    uv += mesh.uv_offset;

    Materials materials = Materials(mesh.material);
    uint texture_index = materials.m.texture_index;
    vec3 texture_color = texture(textures[nonuniformEXT(texture_index)], uv).xyz;

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
