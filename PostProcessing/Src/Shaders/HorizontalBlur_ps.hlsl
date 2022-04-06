#include "Common.hlsli"

Texture2D shaderTexture;
SamplerState SampleType;


static const float kernelOffsets[3] = { 0.0f, 1.3846153846f, 3.2307692308f };
static const float BlurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };


float4 main(PostProcessingInput input) : SV_TARGET
{
    
    //float4 textureColour = shaderTexture.Sample(SampleType, (input.sceneUV / gViewportWidth)) * BlurWeights[0];
    
    
    //for (int i = 1; i < 3; ++i)
    //{
    //    textureColour += shaderTexture.Sample(SampleType, float2(input.sceneUV + float2(0.0f, kernelOffsets[i])) / gViewportWidth) * BlurWeights[i];
    //    textureColour += shaderTexture.Sample(SampleType, float2(input.sceneUV - float2(0.0f, kernelOffsets[i])) / gViewportWidth) * BlurWeights[i];
    //}
    float4 textureColour = shaderTexture.Sample(SampleType, (input.sceneUV));
    
    return float4(textureColour);
    //float weight0, weight1, weight2, weight3, weight4;
    //float normalization;
    //float4 color;
    
    //// Create the weights that each neighbor pixel will contribute to the blur.
    //weight0 = 1.0f;
    //weight1 = 0.9f;
    //weight2 = 0.55f;
    //weight3 = 0.18f;
    //weight4 = 0.1f;
    
    
    //// Create a normalized value to average the weights out a bit.
    //normalization = (weight0 + 2.0f * (weight1 + weight2 + weight3 + weight4));

    //// Normalize the weights.
    //weight0 = weight0 / normalization;
    //weight1 = weight1 / normalization;
    //weight2 = weight2 / normalization;
    //weight3 = weight3 / normalization;
    //weight4 = weight4 / normalization;
    
    // // Initialize the color to black.
    //color = float4(0.0f, 0.0f, 0.0f, 0.0f);

    //// Add the nine horizontal pixels to the color by the specific weight of each.
    //color += shaderTexture.Sample(SampleType, input.texCoord1) * weight4;
    //color += shaderTexture.Sample(SampleType, input.texCoord2) * weight3;
    //color += shaderTexture.Sample(SampleType, input.texCoord3) * weight2;
    //color += shaderTexture.Sample(SampleType, input.texCoord4) * weight1;
    //color += shaderTexture.Sample(SampleType, input.texCoord5) * weight0;
    //color += shaderTexture.Sample(SampleType, input.texCoord6) * weight1;
    //color += shaderTexture.Sample(SampleType, input.texCoord7) * weight2;
    //color += shaderTexture.Sample(SampleType, input.texCoord8) * weight3;
    //color += shaderTexture.Sample(SampleType, input.texCoord9) * weight4;
    // // Set the alpha channel to one.
    //color.a = 0.4f;

        //return color;
    
}