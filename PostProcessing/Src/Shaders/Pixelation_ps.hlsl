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
    //Current position of the pixel
    float2 position = input.sceneUV;
    
    //scale the position to the width and height of the new value 
    position *= float2(gPixelWidth, gPixelHeight);
    
    //Floor the value 
    position = floor(position);
    
    //scale the position back donwto the width and height of the original image
    position /= float2(gPixelWidth, gPixelHeight);
    
    //sample the texture at the new position
    float4 colour = SceneTexture.Sample(PointSample, position);
    
    //Calculate the luminance value of the pixel's colour by performing a dot product between the pixel's colour and the and the luminance vector
    float luminance = dot(colour.rgb, gLuminanceWeights);
    
    //return the correct shade of green based on the luminance value of the pixel
    if (luminance > 0.75f)
    {
        colour.rgb = float3(0.7333f, 0.8118f, 0.3647f);
    }
    else if (luminance > 0.5f)
    {
        colour.rgb = float3(0.5412f, 0.6706f, 0.0588f);
    }
    else if (luminance > 0.25f)
    {
        colour.rgb = float3(0.1882f, 0.3804f, 0.1882f);
    }
    else
    {
        colour.rgb = float3(0.0588f, 0.2196f, 0.0588f);
    }
        
    //Return the final colour
    return float4(colour.rgb, luminance);
}