#define MSAASamples 16
static const float2 SubSampleOffsets[MSAASamples] =
{
    float2(+0.0625f, +0.0625f),
    float2(-0.0625f, -0.1875f),
    float2(-0.1875f, +0.1250f),
    float2(+0.2500f, -0.0625f),

    float2(-0.3125f, -0.1250f),
    float2(+0.1250f, +0.3125f),
    float2(+0.3125f, +0.1875f),
    float2(+0.1875f, -0.3125f),

    float2(-0.1250f, +0.3750f),
    float2(+0.0000f, -0.4375f),
    float2(-0.2500f, -0.3750f),
    float2(-0.3750f, +0.2500f),

    float2(-0.5000f, +0.0000f),
    float2(+0.4375f, -0.2500f),
    float2(+0.3750f, +0.4375f),
    float2(-0.4375f, -0.5000f),
};
#include "screen_ps_core.h"