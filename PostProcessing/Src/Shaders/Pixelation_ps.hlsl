
#include "Common.hlsli"

Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0);


float4 main(PostProcessingInputWithNeighbouringPixels input) : SV_TARGET
{
    float width =  64;
    float height = 64;
    
    float2 position = input.sceneUV;
    position *= float2(gPixelWidth, gPixelHeight);
    position = floor(position);
    position /= float2(gPixelWidth, gPixelHeight);
    float4 colour = SceneTexture.Sample(PointSample, position);
    
    
        
	return colour;
}