#include "Common.hlsli"

Texture2D shaderTexture;
SamplerState SampleType;


static const float kernelOffsets[3] = { 0.0f, 1.3846153846f, 3.2307692308f };
static const float BlurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };
uint loopcount = 1;

float4 main(PostProcessingInput input) : SV_TARGET
{   
    float3 textureColour = shaderTexture.Sample(SampleType, (input.sceneUV)).rgb * BlurWeights[0];
    
    
    for (int i = 1; i < 3; ++i)
    {
        float normalizedOffset = float2(0.0f, kernelOffsets[i]) / gBlurHeight;
        textureColour += shaderTexture.Sample(SampleType, input.sceneUV + normalizedOffset).rgb * BlurWeights[i];
        textureColour += shaderTexture.Sample(SampleType, input.sceneUV - normalizedOffset).rgb * BlurWeights[i];
    }
    
    return float4(textureColour, 1.0f);
}