#include "Common10.fxh"

// Color texture.
Texture2D colorTexture : SourceTexture
<
	string UIName = "Color Texture";
> = NULL;

// Color texture sampler.
SamplerState colorSampler
{
   FILTER = MIN_MAG_MIP_POINT;
};

// Pixel shader output.
struct PS_OUTPUT
{
	float4 color : SV_TARGET;
};

// Color pixel shader.
PS_OUTPUT PS_BlitColor(VS_TO_PS_ScreenQuad In)
{
	PS_OUTPUT outputStruct;

	// Read the color.
	float4 output = colorTexture.Sample(colorSampler, In.UV);

	// Divide by iterations.
	output.r /= output.a;
	output.g /= output.a;
	output.b /= output.a;
	output.a = 1;

	outputStruct.color = output;

	return outputStruct;
}

// The main technique.
technique10 Main
{
	pass p0
	{
		SetVertexShader(CompileShader(vs_4_0, VS_ScreenQuad()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, PS_BlitColor()));
	}
}
