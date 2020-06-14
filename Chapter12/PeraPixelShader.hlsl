#include "PeraTypes.hlsli"

float4 PeraPS(Output input) : SV_TARGET
{
    // return float4(input.uv, 1, 1);

    // return tex.Sample(smp, input.uv);

    float4 col = tex.Sample(smp, input.uv);

    // モノクロ化
    float Y = dot(col.rgb, float3(0.299, 0.587, 0.114));
    return float4(Y, Y, Y, 1);

    // 色反転
    // return float4(float3(1.0f, 1.0f, 1.0f) - col.rgb, col.a);

    // 色の階調を下げる
    // return float4(col.rgb - fmod(col.rgb, 0.25f), col.a);

    // ぼかし
    /*
    float w, h, level;
    tex.GetDimensions(0, w, h, level);

    float dx = 1.0f / w;
    float dy = 1.0f / h;
    float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

    ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)); // 左上
    ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)); // 上
    ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)); // 右上
    ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)); // 左
    ret += tex.Sample(smp, input.uv); // 自身
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)); // 右
    ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)); // 左下
    ret += tex.Sample(smp, input.uv + float2(0, 2 * dy)); // 下
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)); // 右下

    return ret / 9.0f;
    */

    // エンボス加工
    /*
    float w, h, level;
    tex.GetDimensions(0, w, h, level);

    float dx = 1.0f / w;
    float dy = 1.0f / h;
    float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

    ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 2; // 左上
    ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)); // 上
    ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)); // 左
    ret += tex.Sample(smp, input.uv); // 自身
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // 右
    ret += tex.Sample(smp, input.uv + float2(0, 2 * dy)) * -1; // 下
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * -2; // 右下

    // モノクロ化
    float Y = dot(ret.rgb, float3(0.299, 0.587, 0.114));
    return float4(Y, Y, Y, 1);
    */

    // シャープネス
    /*
    float w, h, level;
    tex.GetDimensions(0, w, h, level);

    float dx = 1.0f / w;
    float dy = 1.0f / h;
    float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

    ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // 上
    ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)) * -1; // 左
    ret += tex.Sample(smp, input.uv) * 5; // 自身
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // 右
    ret += tex.Sample(smp, input.uv + float2(0, 2 * dy)) * -1; // 下

    return ret;
    */
    
    // 輪郭線抽出
    /*
    float w, h, level;
    tex.GetDimensions(0, w, h, level);

    float dx = 1.0f / w;
    float dy = 1.0f / h;
    float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

    ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // 上
    ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)) * -1; // 左
    ret += tex.Sample(smp, input.uv) * 4; // 自身
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // 右
    ret += tex.Sample(smp, input.uv + float2(0, 2 * dy)) * -1; // 下

    float Y = dot(ret.rgb, float3(0.299, 0.587, 0.114));
    Y = pow(1.0f - Y, 10.0f);
    Y = step(0.2, Y);
    ret = float4(Y, Y, Y, 1.0);
    return float4(float3(1.0f, 1.0f, 1.0f) - ret.rgb, ret.a);
    */

    // ガウシアンぼかし（本格版）

}