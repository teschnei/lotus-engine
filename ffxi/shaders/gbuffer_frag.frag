#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(binding = 2, set = 1) uniform sampler2D textures[];

layout(binding = 3, set = 1) uniform MaterialInfo
{
    Material m;
} materials[];

layout(binding = 4, set = 1) buffer readonly MeshInfo
{
    Mesh m[];
} meshInfo;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec4 pos;
layout(location = 5) in vec4 prevPos;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outFaceNormal;
layout(location = 3) out vec4 outAlbedo;
layout(location = 4) out uint outMaterialIndex;
layout(location = 5) out uint outLightType;
layout(location = 6) out vec4 outMotionVector;

layout(push_constant) uniform PushConstant
{
    uint mesh_index;
} push;

void main() {
    outPosition = vec4(fragPos, 1.0);
    outNormal = vec4(normal, 1.0);
    vec3 dx = dFdx(fragPos);
    vec3 dy = dFdy(fragPos);
    vec3 cross_vec = normalize(cross(dx, dy));
    if ((dot(cross_vec, normal)) < 0)
        cross_vec = -cross_vec;

    Mesh meshinfo = meshInfo.m[push.mesh_index];
    Material mat = materials[meshinfo.material_index].m;

    outFaceNormal = vec4(cross_vec, 1.0);
    outAlbedo = texture(textures[mat.texture_index], fragTexCoord);
    outAlbedo.rgb *= fragColor;
    
    /*
    if (outAlbedo.a > 1.f/32.f)
        outAlbedo.a = 1;
    else
        outAlbedo.a = 0;
    */
    outAlbedo.a = 1;
        
    outMaterialIndex = push.mesh_index;
    outLightType = mat.light_type;
    vec2 curScreenPos = (pos.xy / pos.w) * 0.5 + 0.5;
    vec2 prevScreenPos = (prevPos.xy / prevPos.w) * 0.5 + 0.5;
    outMotionVector.xy = vec2(curScreenPos - prevScreenPos);
    outMotionVector.zw = vec2(pos.z, prevPos.z);
}
