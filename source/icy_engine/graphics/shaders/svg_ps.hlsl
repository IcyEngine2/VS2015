struct input_type
{
    float4 coord : SV_POSITION;
    float4 color : COLOR;
};
struct output_type
{
    float4 color : SV_TARGET;
};

void main(in input_type input, out output_type output)
{
    output.color = input.color;
}