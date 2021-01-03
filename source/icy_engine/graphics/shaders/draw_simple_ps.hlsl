struct InputType
{
    float4 wcoord : SV_POSITION;
    float4 colors : COLOR0;
    float2 tcoord : TEXCOORD0;
};
struct OutputType
{
    float4 color : SV_Target0;
};

Texture2D InputTexture : register(t0);
SamplerState LinearTextureSampler : register(s0);

void main(in InputType input, out OutputType output)
{
    output.color = input.colors;
    //input.colors* InputTexture.Sample(LinearTextureSampler, input.tcoord);
}