#include"BasicType.hlsli"
Texture2D<float4> tex:register(t0);//0番スロットに設定されたテクスチャ
SamplerState smp:register(s0);//0番スロットに設定されたサンプラ

float4 BasicPS(BasicType input ) : SV_TARGET{
	float3 light = normalize(float3(1,-1,1)); // 光源ベクトル
	float brightness = dot(-light, input.normal); // 光源の逆ベクトルと法線の内積を取る
	// ランバートの余弦則→物体表面の輝度は光源ベクトルと法線ベクトルの成す角をθとしたときのcosθに比例する
	return float4(brightness, brightness, brightness, 1) * diffuse;
}