#define MSAASamples 8
static const float2 SubSampleOffsets[MSAASamples] =
{
    float2(+0.0625f, -0.1875f),
    float2(-0.0625f, +0.1875f),
    float2(+0.3125f, +0.0625f),
    float2(-0.1875f, -0.3125f),
    float2(-0.3125f, +0.3125f),
    float2(-0.4375f, -0.0625f),
    float2(+0.1875f, +0.4375f),
    float2(+0.4375f, -0.4375f),
};
#include "screen_ps_core.h"