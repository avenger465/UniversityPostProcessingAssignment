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

// This shader also uses a texture filled with noise
Texture2D    NoiseMap      : register(t1);
SamplerState TrilinearWrap : register(s1);

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------


static const float kernelOffsets[3] = { 0.0f, 1.3846153846f, 3.2307692308f };
static const float BlurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };
// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{ 
    float4 Colour = SceneTexture.Sample(PointSample, input.sceneUV);// * .5f;
	//Colour += SceneTexture.Sample(PointSample, input.sceneUV + (gBlurOffset)) * .2f;
	
	//float4 textureColour = SceneTexture.Sample(PointSample, (input.sceneUV / gViewportWidth)) * BlurWeights[0];
	//float4 textureColour = SceneTexture.Sample(PointSample, (input.sceneUV));
    
    
 //   for (int i = 1; i < 3; ++i)
 //   {
 //       textureColour += SceneTexture.Sample(PointSample, float2(input.sceneUV + float2(0.0f, kernelOffsets[i])) / gViewportWidth) * BlurWeights[i];
 //       textureColour += SceneTexture.Sample(PointSample, float2(input.sceneUV - float2(0.0f, kernelOffsets[i])) / gViewportWidth) * BlurWeights[i];
 //   }
    //PointSample
	
	
	//// Output final colour
	//return textureColour;
	return Colour;
}