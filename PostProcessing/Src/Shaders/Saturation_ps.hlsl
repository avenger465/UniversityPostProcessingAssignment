//--------------------------------------------------------------------------------------
// Colour Tint Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------
// Just samples a pixel from the scene texture and multiplies it by a fixed colour to tint the scene

#include "Common.hlsli"


//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// The scene has been rendered to a texture, these variables allow access to that texture
Texture2D    SceneTexture : register(t0);
SamplerState PointSample  : register(s0); // We don't usually want to filter (bilinear, trilinear etc.) the scene texture when
                                          // post-processing so this sampler will use "point sampling" - no filtering


//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInputWithNeighbouringPixels input) : SV_Target
{
    float3 LuminanceWeights = float3(0.3086f, 0.6094f, 0.0820f);
    //float3 LuminanceWeights = float3(0.3, 0.3, 0.3);
    float4 outputColour = SceneTexture.Sample(PointSample, input.sceneUV);
    float luminance = dot(outputColour.rgb, LuminanceWeights);
    float4 dstPixel = lerp(luminance, outputColour, saturationLevel);
    dstPixel.a = (outputColour.r + outputColour.g + outputColour.b) / 3;
    return dstPixel;

}