struct InputType
{
    float4 coord : SV_POSITION;
    float2 tex : TEXCOORD0;
};
struct OutputType
{
    float4 color : SV_Target0;
};

Texture2D InputTexture : register(t0);
SamplerState LinearTextureSampler : register(s0);

void main(in InputType input, out OutputType output)
{
    output.color = InputTexture.Sample(LinearTextureSampler, input.tex);
}