#include "Common.hlsli"

//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0);

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

float4 main(PostProcessingInput input) : SV_TARGET
{
    //Sample the colour of the texture at the pixels UV coordinates
    float4 Colour = SceneTexture.Sample(PointSample, input.sceneUV);
    
    //Calculate the distance of the pixel to the center of the texture
    float dist = distance(input.areaUV, float2(0.5f, 0.5f));
    
    //Smoothly transition between the size of the vignette and the size minus the falloff value using the calculated distance
    float vignette = smoothstep(gVignetteSize, gVignetteSize - gVignetteFalloff, dist);
    
    //Perform a linear interpolation between the vignette and 1
    vignette = lerp(1.0f, vignette, gVignetteStrength);
    
    //Clamp the the pixels colour between 0 and 1
    Colour = saturate(Colour * vignette);
      
    //return the final colour
    return Colour;
}