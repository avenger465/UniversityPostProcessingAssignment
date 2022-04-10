#include "Common.hlsli"

//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0);

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

//Distort the pixel coordinates 
float2 distort(float2 pos)
{
    //Create the distortion around a circle
    float power = 1.5f;
    float theta = atan2(pos.y, pos.x);
    float radius = length(pos);
    radius = pow(radius, power);
    pos.x = radius * cos(theta);
    pos.y = radius * sin(theta);

    return 0.5 * (pos + 1.0);
}

float4 main(PostProcessingInput input) : SV_TARGET
{
    //Calculate the pixel position in the scene texture
    float2 xy = 2.0 * input.areaUV - 1.0;
    
    //Get the length of the pixels UV coordinates
    float d = length(xy);

    //Calculate the distoted UV coordinates
    float2 uv = distort(xy);
    
    //Calculate the colour of the texture at the new UV coordinates
    //and if the value is greater than 1 set the pixel to black create a circle effect
    float4 Colour = SceneTexture.Sample(PointSample, uv);
    if (d >= 1.0)
    {
        Colour.rgb = float3(0, 0, 0);
    }
    
    //Return the final colour
    return Colour;
}