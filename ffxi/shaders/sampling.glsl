//from https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/blob/master/ray_tracing_gltf/shaders/sampling.glsl

// Generate a random unsigned int from two unsigned int values, using 16 pairs
// of rounds of the Tiny Encryption Algorithm. See Zafar, Olano, and Curtis,
// "GPU Random Numbers via the Tiny Encryption Algorithm"
uint tea(uint val0, uint val1)
{
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  for(uint n = 0; n < 16; n++)
  {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev)
{
  uint LCG_A = 1664525u;
  uint LCG_C = 1013904223u;
  prev       = (LCG_A * prev + LCG_C);
  return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float rnd(inout uint prev)
{
  return (float(lcg(prev)) / float(0x01000000));
}

#define M_PI 3.141592

//-------------------------------------------------------------------------------------------------
// Sampling
//-------------------------------------------------------------------------------------------------

// Randomly sampling around +Z
vec3 samplingHemisphere(inout uint seed, in vec3 x, in vec3 y, in vec3 z)
{
  float r1 = rnd(seed);
  float r2 = rnd(seed);
  float sq = sqrt(1.0 - r2);

  vec3 direction = vec3(cos(2 * M_PI * r1) * sq, sin(2 * M_PI * r1) * sq, sqrt(r2));
  direction      = direction.x * x + direction.y * y + direction.z * z;

  return normalize(direction);
}

// Randomly sampling in direction of +Z
vec3 samplingCone(inout uint seed, inout float pdf, in vec3 x, in vec3 y, in vec3 z, in float radius, in float length_sq)
{
    float cos_a_max = sqrt(1 - radius * radius / length_sq);
    float r1 = rnd(seed);
    float r2 = rnd(seed);
    float cos_a = 1 - r1 + (r1 * cos_a_max);
    float sin_a = sqrt(1 - cos_a * cos_a);
    float phi = 2 * M_PI * r2;
    vec3 direction = x * cos(phi) * sin_a + y * sin(phi) * sin_a + z * cos_a;
    pdf = 1 / (2 * M_PI * (1 - cos_a_max));
    return normalize(direction);
}

// Return the tangent and binormal from the incoming normal
void createCoordinateSystem(in vec3 N, out vec3 Nt, out vec3 Nb)
{
  if(abs(N.x) > abs(N.y))
    Nt = vec3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
  else
    Nt = vec3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
  Nb = cross(N, Nt);
}
