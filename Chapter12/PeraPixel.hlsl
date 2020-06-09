#include "PeraHeader.hlsli"

float ps(Output input) : SV_Target{
    return float4(input.uv, 1, 1);
}