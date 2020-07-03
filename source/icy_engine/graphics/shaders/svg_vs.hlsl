struct input_type
{
    float3 coord : SV_POSITION;
    float4 color : COLOR;
};
struct output_type
{
    float4 coord : SV_POSITION;
    float4 color : COLOR;
};

void main(in input_type input, out output_type output)
{
    output.coord = float4(input.coord, 1);
    output.color = input.color;
}