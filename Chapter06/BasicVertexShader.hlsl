#include"BasicType.hlsli"

// �萔�o�b�t�@
cbuffer cbuff0 : register(b0) {
	matrix mat; // �ϊ��s��
}

BasicType BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD) {
	BasicType output;//�s�N�Z���V�F�[�_�֓n���l
	output.svpos = mul(mat, pos);
	output.uv = uv;
	return output;
}