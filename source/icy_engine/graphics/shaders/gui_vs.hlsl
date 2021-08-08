struct input_type
{
	float2 pos : POSITION;
	float2 tex : TEXCOORD0;
	float4 col : COLOR0;
};
struct output_type
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 tex : TEXCOORD0;
};

void main(in input_type input, out output_type output)
{
	output.pos = float4(input.pos.xy, 0, 1);
	output.col = input.col;
	output.tex = input.tex;
}