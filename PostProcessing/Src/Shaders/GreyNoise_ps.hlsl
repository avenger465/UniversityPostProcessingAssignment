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

//float Pixels[13] =
//{
//    -6,
//   -5,
//   -4,
//   -3,
//   -2,
//   -1,
//    0,
//    1,
//    2,
//    3,
//    4,
//    5,
//    6,
//};

//float BlurWeights[13] =
//{
//    0.002216,
//   0.008764,
//   0.026995,
//   0.064759,
//   0.120985,
//   0.176033,
//   0.199471,
//   0.176033,
//   0.120985,
//   0.064759,
//   0.026995,
//   0.008764,
//   0.002216,
//};
//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInputWithNeighbouringPixels input) : SV_Target
{ 
    float4 Colour = SceneTexture.Sample(PointSample, input.sceneUV) * .5f;
	Colour += SceneTexture.Sample(PointSample, input.sceneUV + (gBlurOffset)) * .2f;
	
	
	//// Output final colour
	return Colour;
}