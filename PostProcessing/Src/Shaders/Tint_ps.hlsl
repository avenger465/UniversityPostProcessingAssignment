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

float Min(float x, float y)
{
    return min(x, y);
}
float Max(float x, float y)
{
    return max(x, y);
}

float3 HUEtoRGB(float hue)
{
    hue = frac(hue);
    float r = abs(hue * 6 - 3) - 1;
    float g = 2 - abs(hue * 6 - 2);
    float b = 2 - abs(hue * 6 - 4);

    float3 rgb = float3(r, g, b);
    rgb = saturate(rgb);
    return rgb;

}

float3 HSLtoRGB(float3 HSL)
{
    float3 rgb = HUEtoRGB(HSL.x);
    rgb = lerp(1, rgb, HSL.y);
    rgb = rgb * HSL.z;
    return rgb;
}

float3 RGBtoHSL(float3 RGB)
{
    float maxComponent = Max(RGB.r, Max(RGB.g, RGB.b));
    float minComponent = Min(RGB.r, Min(RGB.g, RGB.b));
    float diff = maxComponent - minComponent;
    float hue = 0;
    if (maxComponent == RGB.r)
    {
        hue = 0 + (RGB.g - RGB.b) / diff;
    }
    else if (maxComponent == RGB.g)
    {
        hue = 2 + (RGB.b - RGB.r) / diff;
    }
    else if (maxComponent == RGB.b)
    {
        hue = 4 + (RGB.r - RGB.g) / diff;
    }
    hue = frac(hue / 6);

    float saturation = diff / maxComponent;
    float value = maxComponent;
    return float3(hue, saturation, value);
 
}

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInputWithNeighbouringPixels input) : SV_Target
{
	// Sample a pixel from the scene texture and multiply it with the tint colour (comes from a constant buffer defined in Common.hlsli)
	float3 colour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    //float3 Colour1 = RGBtoHSL(gTintColour1);
    //float3 Colour2 = RGBtoHSL(gTintColour2);

    colour += lerp(gTintColour1, gTintColour2, input.sceneUV.y);
    float3 FinalColour = RGBtoHSL(colour);
    
    FinalColour.x += gHeatHazeTimer / 10;
    
    FinalColour = HSLtoRGB(FinalColour);
    
	
	return float4(FinalColour, 1.0f);
}