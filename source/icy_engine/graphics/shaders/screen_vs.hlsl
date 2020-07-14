struct input_type
{
    uint index : SV_VERTEXID;
};
struct output_type
{
    float4 coord : SV_POSITION;
    float2 tex : TEXCOORD0;
};

void main(in input_type input, out output_type output)
{
    if (input.index == 0)
    {
        output.coord = float4(-1.0f, 1.0f, 0.0f, 1.0f);
        output.tex = float2(0.0f, 0.0f);
    }
    else if (input.index == 1)
    {
        output.coord = float4(3.0f, 1.0f, 0.0f, 1.0f);
        output.tex = float2(1.0f, 0.0f);
    }
    else
    {
        output.coord = float4(-1.0f, -3.0f, 0.0f, 1.0f);
        output.tex = float2(0.0f, 1.0f);
    }    
}