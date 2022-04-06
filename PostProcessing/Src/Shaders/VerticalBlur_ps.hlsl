#include "Common.hlsli"

Texture2D shaderTexture;
SamplerState SampleType;


static const float kernelOffsets[3] = { 0.0f, 1.3846153846f, 3.2307692308f };
static const float BlurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };
uint loopcount = 1;

float4 main(PostProcessingInput input) : SV_TARGET
{
    
    //float4 textureColour = shaderTexture.Sample(SampleType, (input.sceneUV / gViewportHeight)) * BlurWeights[0];
    
    
    //for (int i = 1; i < 3; ++i)
    //{
    //    textureColour += shaderTexture.Sample(SampleType, float2(input.sceneUV + float2(0.0f, kernelOffsets[i])) / gViewportHeight) * BlurWeights[i];
    //    textureColour += shaderTexture.Sample(SampleType, float2(input.sceneUV - float2(0.0f, kernelOffsets[i])) / gViewportHeight) * BlurWeights[i];
    //}
    
    float4 textureColour = shaderTexture.Sample(SampleType, (input.sceneUV));
    
    return float4(textureColour);
    
    
    //float3 textureColour = float3(1.0f, 0.0f, 0.0f);
    //float2 uv = input.sceneUV;
    //if (uv.x > (gBlurOffset + 0.005f))
    //{
    //    textureColour = shaderTexture.Sample(SampleType, uv).rgb * BlurWeights[0];
    //    for (int i = 1; i < 3; ++i)
    //    {
    //        float2 normalisedOffset = float2(0.0f, kernelOffsets[i]) / gViewportWidth;
    //        textureColour += shaderTexture.Sample(SampleType, uv + normalisedOffset).rgb * BlurWeights[i];
    //        textureColour += shaderTexture.Sample(SampleType, uv - normalisedOffset).rgb * BlurWeights[i];
    //    }

    //}
    //else if (uv.x <= (gBlurOffset - 0.005f))
    //{
    //    textureColour = shaderTexture.Sample(SampleType, uv).rgb;
    //}
    
    //const float3 textureColourOrig = textureColour;
    //for (uint i = 0; i < loopcount; ++i)
    //{
    //    textureColour += textureColourOrig;
    //}
    
    //if (loopcount > 0)
    //{
    //    textureColour /= loopcount + 1;
    //}
    
    //return float4(textureColour, 1.0f);
}