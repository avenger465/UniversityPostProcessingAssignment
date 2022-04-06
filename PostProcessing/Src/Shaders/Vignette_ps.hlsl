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

float4 main(PostProcessingInputWithNeighbouringPixels input) : SV_TARGET
{
    float4 Colour = SceneTexture.Sample(PointSample, input.sceneUV);
    
    //Vignette
    float dist = distance(input.sceneUV, float2(0.5f, 0.5f));
    
    float vignette = smoothstep(gVignetteSize, gVignetteSize - gVignetteFalloff, dist);
    vignette = lerp(1.0f, vignette, gVignetteStrength);
    
    Colour = saturate(Colour * vignette);
      
    return Colour;
}