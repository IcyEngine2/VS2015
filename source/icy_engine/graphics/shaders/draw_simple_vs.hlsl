struct input_type
{
    float4 wcoord : POSITION0;
    float4 colors : COLOR0;
    float2 tcoord : TEXCOORD0;
    uint index : SV_VERTEXID;
};
struct output_type
{
    float4 wcoord : SV_POSITION;
    float4 colors : COLOR0;
    float2 tcoord : TEXCOORD0;
};

void main(in input_type input, out output_type output)
{
    output.wcoord = input.wcoord;
    output.colors = input.colors;
    output.tcoord = input.tcoord;
    /*
    if (input.index == 0)
    {
        output.wcoord = float4(-1.0f, 1.0f, 0.0f, 1.0f);
    }
    else if (input.index == 1)
    {
        output.wcoord = float4(3.0f, 1.0f, 0.0f, 1.0f);
    }
    else
    {
        output.wcoord = float4(-1.0f, -3.0f, 0.0f, 1.0f);
    }*/
}