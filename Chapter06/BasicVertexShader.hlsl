#include"BasicType.hlsli"

// 定数バッファ
cbuffer cbuff0 : register(b0) {
	matrix mat; // 変換行列
}

BasicType BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD) {
	BasicType output;//ピクセルシェーダへ渡す値
	output.svpos = mul(mat, pos);
	output.uv = uv;
	return output;
}