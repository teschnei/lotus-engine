struct Mesh
{
    int vertex_offset;
    int index_offset;
    uint indices;
    int material_index;
    vec3 scale;
    uint billboard;
    vec4 colour;
};

struct Material
{
    float specular_exponent;
    float specular_intensity;
    uint light_type;
};

