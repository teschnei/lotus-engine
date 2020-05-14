struct Mesh
{
    int vec_index_offset;
    int tex_offset;
    float specular_exponent;
    float specular_intensity;
    vec4 color;
    vec3 scale;
    uint billboard;
    uint light_type;
    uint indices;
};

