#define MSAASamples 2
static const float2 SubSampleOffsets[MSAASamples] =
{
    float2(+0.25f, +0.25f),
    float2(-0.25f, -0.25f),
};
#include "screen_ps_core.h"