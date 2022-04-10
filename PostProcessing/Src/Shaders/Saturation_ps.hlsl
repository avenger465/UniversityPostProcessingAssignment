#include "Common.hlsli"

//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// The scene has been rendered to a texture, these variables allow access to that texture
Texture2D    SceneTexture : register(t0);
SamplerState PointSample  : register(s0); // We don't usually want to filter (bilinear, trilinear etc.) the scene texture when
                                          // post-processing so this sampler will use "point sampling" - no filtering
//Texture that will be used with alpha blending to cut out the shape of the hole in the wall
Texture2D AlphaMap : register(t1);
SamplerState TrilinearWrap : register(s1);
//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target

{
    //get the colour value of the alpha map
    float4 alphaMap = AlphaMap.Sample(TrilinearWrap, input.areaUV);
    
    //Sample the colour from the sceneTexture from the current UV coordinates
    float4 outputColour = SceneTexture.Sample(PointSample, input.sceneUV);
    
    //Calculate the luminance value of the pixel's colour by performing a dot product between the pixel's colour and the and the luminance vector
    float luminance = dot(outputColour.rgb, gLuminanceWeights);
    
    //Linear interpolate between the original colour and the tint colour based on the luminance value
    float4 dstPixel = lerp(luminance, outputColour, saturationLevel);
    dstPixel.a = (outputColour.r + outputColour.g + outputColour.b) / 3;
    
    //if the value is greater than 0.1, then we want to cut out the shape of the hole in the wall
	//by discarding this pixel
    if (alphaMap.r > 0.1f)
    {
        discard;

    }
    
    //return the colour of the pixel
    return dstPixel;
}