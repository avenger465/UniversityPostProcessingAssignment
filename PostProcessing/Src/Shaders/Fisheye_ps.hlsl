#include "Common.hlsli"

Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0);

float2 distort(float2 pos)
{
    float power = 2;
    float theta = atan2(pos.y, pos.x);
    float radius = length(pos);
    radius = pow(radius, power);
    pos.x = radius * cos(theta);
    pos.y = radius * sin(theta);

    return 0.5 * (pos + 1.0);
}

float4 main(PostProcessingInput input) : SV_TARGET
{
    float2 xy = 2.0 * input.sceneUV - 1.0;
    
    float d = length(xy);

    float2 uv = distort(xy);
    float4 Colour = SceneTexture.Sample(PointSample, uv);
    if (d >= 1.0)
    {
        Colour.rgb = float3(0, 0, 0);
    }
    
    return Colour;
}