#define MSAASamples 4
static const float2 SubSampleOffsets[MSAASamples] =
{
    float2(-0.125f, -0.375f),
    float2(+0.375f, -0.125f),
    float2(-0.375f, +0.125f),
    float2(+0.125f, +0.375f),
};
#include "msaa_ps_core.h"