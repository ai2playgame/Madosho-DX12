//���_�V�F�[�_���s�N�Z���V�F�[�_�ւ̂����Ɏg�p����
//�\����
struct BasicType {
	float4 svpos:SV_POSITION; // �V�X�e���p���_���W
	float4 normal:NORMAL; // �@���x�N�g��
	float2 uv:TEXCOORD; // UV�l
};

//�萔�o�b�t�@
cbuffer cbuff0 : register(b0) {
	matrix world;//���[���h�ϊ��s��
	matrix viewproj;//�r���[�v���W�F�N�V�����s��
};

// �萔�o�b�t�@1
// �}�e���A���p
cbuffer Material : register(b1) {
	float4 diffuse; // �f�B�t���[�Y�F
	float4 specular; // �X�y�L����
	float3 ambient; // �A���r�G���g
}