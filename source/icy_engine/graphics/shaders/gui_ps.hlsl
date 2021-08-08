struct input_type
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 tex : TEXCOORD0;
};
struct output_type
{
	float4 col : SV_TARGET0;
};

Texture2D InputTexture : register(t0);
SamplerState LinearTextureSampler : register(s0);

void main(in input_type input, out output_type output)
{
	output.col = input.col* InputTexture.Sample(LinearTextureSampler, input.tex);
}