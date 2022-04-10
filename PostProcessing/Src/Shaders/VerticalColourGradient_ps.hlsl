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

float3 HUEtoRGB(float hue)
{
    //returns the decimal part of the input
    hue = frac(hue);
    
    //get the indivual RGB components from the Hue Component
    float r = abs(hue * 6 - 3) - 1;
    float g = 2 - abs(hue * 6 - 2);
    float b = 2 - abs(hue * 6 - 4);

    //Convert the RGB values to the range 0-1
    float3 rgb = float3(r, g, b);
    rgb = saturate(rgb);
    
    //Return the RGB values
    return rgb;

}

float3 HSLtoRGB(float3 HSL)
{
    //Return the RGB value from the Hue compoment of the HSL color
    float3 rgb = HUEtoRGB(HSL.x);
    
    //Linear interpolate between 1 and the RGB value along the Saturation component of the HSL color
    rgb = lerp(1, rgb, HSL.y);
    
    //Multiply the RGB value by the Lightness component of the HSL color
    rgb = rgb * HSL.z;
    
    //Return the final colour
    return rgb;
}

float3 RGBtoHSL(float3 RGB)
{
    //The maximum and minimum RGB values
    float maxComponent = max(RGB.r, max(RGB.g, RGB.b));
    float minComponent = min(RGB.r, min(RGB.g, RGB.b));
    
    //range
    float diff = maxComponent - minComponent;
    
    //initialise Hue
    float hue = 0;
    
    //Calculate the Hue value based on the maxComponent
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
    
    //Convert the Hue value to degrees
    hue = frac(hue / 6);

    //Calculate the saturation value
    float saturation = diff / maxComponent;
    
    //Calculate the lightness value
    float value = maxComponent;
    
    //Return the HSL values
    return float3(hue, saturation, value);
 
}

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{
	// Sample the colour from the scene texture 
	float3 colour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;

    //Linear interpolate between the two colours along the y axis and add it to the Colour of the scene
    colour += lerp(gTintColour1, gTintColour2, input.sceneUV.y);
    
    //Convert the colour to HSL
    float3 FinalColour = RGBtoHSL(colour);
    
    //Multiply the hue value of the colour to produce a gradually changing colour
    FinalColour.x += gUnderwaterEffect / 10;
    
    //Convert the colour back to RGB
    FinalColour = HSLtoRGB(FinalColour);
    
    //Return the final colour
	return float4(FinalColour, 1.0f);
}