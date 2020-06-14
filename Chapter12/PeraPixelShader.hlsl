#include "PeraTypes.hlsli"

float4 PeraPS(Output input) : SV_TARGET
{
    // return float4(input.uv, 1, 1);

    // return tex.Sample(smp, input.uv);

    float4 col = tex.Sample(smp, input.uv);

    // ���m�N����
    float Y = dot(col.rgb, float3(0.299, 0.587, 0.114));
    return float4(Y, Y, Y, 1);

    // �F���]
    // return float4(float3(1.0f, 1.0f, 1.0f) - col.rgb, col.a);

    // �F�̊K����������
    // return float4(col.rgb - fmod(col.rgb, 0.25f), col.a);

    // �ڂ���
    /*
    float w, h, level;
    tex.GetDimensions(0, w, h, level);

    float dx = 1.0f / w;
    float dy = 1.0f / h;
    float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

    ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)); // ����
    ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)); // ��
    ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)); // �E��
    ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)); // ��
    ret += tex.Sample(smp, input.uv); // ���g
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)); // �E
    ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)); // ����
    ret += tex.Sample(smp, input.uv + float2(0, 2 * dy)); // ��
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)); // �E��

    return ret / 9.0f;
    */

    // �G���{�X���H
    /*
    float w, h, level;
    tex.GetDimensions(0, w, h, level);

    float dx = 1.0f / w;
    float dy = 1.0f / h;
    float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

    ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 2; // ����
    ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)); // ��
    ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)); // ��
    ret += tex.Sample(smp, input.uv); // ���g
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // �E
    ret += tex.Sample(smp, input.uv + float2(0, 2 * dy)) * -1; // ��
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * -2; // �E��

    // ���m�N����
    float Y = dot(ret.rgb, float3(0.299, 0.587, 0.114));
    return float4(Y, Y, Y, 1);
    */

    // �V���[�v�l�X
    /*
    float w, h, level;
    tex.GetDimensions(0, w, h, level);

    float dx = 1.0f / w;
    float dy = 1.0f / h;
    float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

    ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // ��
    ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)) * -1; // ��
    ret += tex.Sample(smp, input.uv) * 5; // ���g
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // �E
    ret += tex.Sample(smp, input.uv + float2(0, 2 * dy)) * -1; // ��

    return ret;
    */
    
    // �֊s�����o
    /*
    float w, h, level;
    tex.GetDimensions(0, w, h, level);

    float dx = 1.0f / w;
    float dy = 1.0f / h;
    float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

    ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // ��
    ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)) * -1; // ��
    ret += tex.Sample(smp, input.uv) * 4; // ���g
    ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // �E
    ret += tex.Sample(smp, input.uv + float2(0, 2 * dy)) * -1; // ��

    float Y = dot(ret.rgb, float3(0.299, 0.587, 0.114));
    Y = pow(1.0f - Y, 10.0f);
    Y = step(0.2, Y);
    ret = float4(Y, Y, Y, 1.0);
    return float4(float3(1.0f, 1.0f, 1.0f) - ret.rgb, ret.a);
    */

    // �K�E�V�A���ڂ����i�{�i�Łj

}