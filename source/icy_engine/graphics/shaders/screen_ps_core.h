Texture2DMS<float4> InputTexture : register(t0);

static const int SampleRadius = 1;
static const float FilterRadius = 1;

struct InputType
{
    float4 coord : SV_POSITION;
    float2 tex : TEXCOORD0;
};
struct OutputType
{
    float4 color : SV_Target0;
};

void main(in InputType input, out OutputType output)
{
    float3 sum = 0.0f;
    float sumW = 0.0f;

    uint dx;
    uint dy;
    uint sampleCount;
    InputTexture.GetDimensions(dx, dy, sampleCount);

    for (int y = -SampleRadius; y <= SampleRadius; ++y)
    {
        for (int x = -SampleRadius; x <= SampleRadius; ++x)
        {            
            [unroll]
            for (uint subSampleIdx = 0; subSampleIdx < MSAASamples; ++subSampleIdx)
            {
                float2 offset = SubSampleOffsets[subSampleIdx].xy;
                float2 sampleDist = abs(float2(x, y) + offset) / FilterRadius;
                
                if (all(sampleDist <= 1.0f))
                {
                    uint2 texCoord = uint2(input.tex.x * dx, input.tex.y * dy);
                    float3 sample = max(InputTexture.Load(texCoord, subSampleIdx).xyz, 0);
                    float weight =
                        (1.0f - smoothstep(0.0f, 1.0f, sampleDist.x)) *
                        (1.0f - smoothstep(0.0f, 1.0f, sampleDist.y));

                    sum += sample * weight;
                    sumW += weight;
                }
            }
        }
    }
    //output.color = float4(1, 1, 0, 1);
    output.color = float4(max(sum / max(sumW, 0.00001f), 0.0f), 1.0f);
}