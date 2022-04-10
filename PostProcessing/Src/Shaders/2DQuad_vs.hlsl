#include "Common.hlsli"


PostProcessingInput main( uint vertexId : SV_VertexID )
{
    PostProcessingInput output;
    
    const float2 Quad[4] =
    {
        float2(0.0, 0.0), // Top-left
					         float2(0.0, 1.0), // Bottom-left
						     float2(1.0, 0.0), // Top-right
						     float2(1.0, 1.0)
    }; // Bottom-left

	// Look up the above array with the vertex ID (see comment on function)
    float2 quadCoord = Quad[vertexId];

	// Use values passed over from C++ in constant buffer (common.hlsli) to convert to coordinates of area to affect
    float2 areaCoord = gArea2DTopLeft + quadCoord * gArea2DSize;

	// Convert 0->1 UV coordinates to 2D viewport coordinates (-1 -> 1) and flip Y axis
    float2 screenCoord = areaCoord * 2 - 1;
    screenCoord.y = -screenCoord.y;

	// Pass values on to post-processing pixel shader, also set depth value for this post-process (set from C++)
    output.areaUV = quadCoord; // These UVs refer to the area being post-processed (see ascii diagram below)
    output.sceneUV = areaCoord; // These UVs refer to the scene texture (see diagram below)
    output.projectedPosition = float4(screenCoord, gArea2DDepth, 1);
    
	return output;
}