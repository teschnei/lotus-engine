vec4 handleColor(sampler2D s, vec2 texCoord, vec3 fragColor, vec3 fragPos, vec3 normal, vec3 light_dir)
{
    vec3 norm = normalize(normal);
    float theta = clamp(dot(norm, -light_dir), 0.5, 1);
    vec4 tex = texture(s, texCoord);
    tex.rgb = tex.rgb * fragColor * theta;
    return tex;
}

