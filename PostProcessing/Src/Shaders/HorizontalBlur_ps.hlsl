#include "Common.hlsli"

//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

Texture2D shaderTexture;
SamplerState SampleType;

//Kernel offsets taken from the Gaussian blur algorithm
static const float kernelOffsets[3] = { 0.0f, 1.3846153846f, 3.2307692308f };

//Blur values to multiply the sampled colours by
static const float BlurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

float4 main(PostProcessingInput input) : SV_TARGET
{   
    //Multiply the value returned from the texture at the current UV coordinates by the kernel weights
    float3 textureColour = shaderTexture.Sample(SampleType, (input.sceneUV)).rgb * BlurWeights[0];
    
    //loop thruogh the kernel offsets
    for (int i = 1; i < 3; ++i)
    {
        //Normalise the Kernal offsets with the BlurWidth along the X axis
        float normalisedOffset = float2(kernelOffsets[i], 0.0f) / gBlurWidth;

        //add the colour sampled from the texture at the next positive x value from the sceneUV to the original colour
        textureColour += shaderTexture.Sample(SampleType, input.sceneUV + normalisedOffset).rgb * BlurWeights[i];
        
        //add the colour sampled from the texture at the next negative x value from the sceneUV to the original colour
        textureColour += shaderTexture.Sample(SampleType,input.sceneUV -  normalisedOffset).rgb * BlurWeights[i];
    }
    
    //Return the final colour of pixel
    return float4(textureColour, 1.0f);
}