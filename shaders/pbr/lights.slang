implementing pbr;

__include common;
__include sampling;

namespace pbr
{
public struct LightSample
{
    public float3 L;
    public float3 wi;
    public float len;
    public float pdf_inv;
};

interface ILight
{
    Optional<LightSample> SampleLi(float3 point, float2 u);
};

public struct LightSampler<Light> where Light: ILight
{
    public Optional<LightSample> SampleLi(float3 point, float2 u)
    {
        if (let li = light.SampleLi(point, u))
        {
            //do anything?
            return li;
        }
        return none;
    }
    public __init(Light _light) { light = _light; }
    Light light;
};

public struct DistantAreaLight : ILight
{
    Optional<LightSample> SampleLi(float3 point, float2 u)
    {
        float3 difference = position - point;
        float3 direction = normalize(difference);
        float length = length(difference);
        if (length > radius)
        {
            var frame = Frame(direction);
            Sample sample = pbr::SampleCone(u, radius, dot(difference, difference));
            return LightSample(colour, frame.LocalToWorld(sample.wi), length, 1);
        }
        return none;
    }

    public __init(float3 _colour, float3 _position, float _radius)
    {
        colour = _colour;
        position = _position;
        radius = _radius;
    }

    float3 colour;
    float3 position;
    float radius;
}
}
