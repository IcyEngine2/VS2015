struct input_type
{
    float2 coord : POSITION0;
    float2 tex   : TEXCOORD0;
};
struct output_type
{
    float4 coord : SV_POSITION;
    float2 tex   : TEXCOORD0;
};

void main(in input_type input, out output_type output)
{
    output.coord = float4(input.coord, 1, 1);    
    output.tex = input.tex;
}