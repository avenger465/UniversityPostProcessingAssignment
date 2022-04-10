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

// This shader also uses a "distortion" texture, which containts 2D vectors (in R & G) to shift the texture UVs to give a cut-glass impression
Texture2D    DistortMap    : register(t1);
SamplerState TrilinearWrap : register(s1);

//Texture that will be used with alpha blending to cut out the shape of the hole in the wall
Texture2D AlphaMap : register(t2);

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{
	const float lightStrength = 0.015f;
	const float glassDarken = 0.8f;
	
	// Get distort texture colour
    float3 distortTexture = DistortMap.Sample( TrilinearWrap, input.areaUV ).rgb;

	// Get direction (2D vector) to distort UVs from the g & b components of the distortion texture ()
	float2 distortVector = distortTexture.gb;
	
	// Converting from UV 0->1 range to -0.5->0.5 range
	distortVector -= float2(0.5f, 0.5f);
			
	// Simple fake diffuse lighting formula based on 2D vector, light coming from top-left
	float light = dot( normalize(distortVector), float2(0.707f, 0.707f) ) * lightStrength;
	
	// Get final colour by adding fake light colour plus scene texture sampled with distort texture offset
	float3 outputColour = light + SceneTexture.Sample(PointSample, input.sceneUV + gDistortLevel * distortVector).rgb * glassDarken;

	//get the colour value of the alpha map
    float alpha = AlphaMap.Sample(TrilinearWrap, input.areaUV).r;
	
	//if the value is greater than 0.1, then we want to cut out the shape of the hole in the wall
	//by discarding this pixel
    if (alpha > 0.1f)
    {
        discard;
    }
    
	//Return the final colour of the pixel
    return float4( outputColour, 1.0f );

}