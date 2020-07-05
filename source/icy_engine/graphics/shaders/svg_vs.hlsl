struct input_type
{
    float3 coord : POSITION;
    float4 color : COLOR;
};
struct output_type
{
    float4 coord : SV_POSITION;
    float4 color : COLOR;
};
cbuffer global_type
{
    float2 size;
};

void main(in input_type input, out output_type output)
{
    output.coord = float4(float2(-1, 1) + input.coord.xy / size * float2(2, -2), input.coord.z, 1);    
    output.color = input.color;
}