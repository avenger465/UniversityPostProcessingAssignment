
#include "Common.hlsli"
Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0);

float4 main(PostProcessingInput input) : SV_TARGET
{
	
    float4 textureColor;
    float4 fogColor;
    float4 finalColor;

	
    // Sample the texture pixel at this location.
    textureColor = SceneTexture.Sample(PointSample, input.areaUV);
    
    // Set the color of the fog to grey.
    fogColor = float4(0.5f, 0.5f, 0.5f, 1.0f);

    // Calculate the final color using the fog effect equation.
    finalColor = textureColor; //input.FogFactor * textureColor + (1.0 - input.FogFactor) * fogColor;

    return finalColor;
}