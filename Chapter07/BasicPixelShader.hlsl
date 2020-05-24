#include"BasicType.hlsli"
Texture2D<float4> tex:register(t0);//0�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`��
SamplerState smp:register(s0);//0�ԃX���b�g�ɐݒ肳�ꂽ�T���v��

float4 BasicPS(BasicType input ) : SV_TARGET{
	float3 light = normalize(float3(1,-1,1)); // �����x�N�g��
	float brightness = dot(-light, input.normal); // �����̋t�x�N�g���Ɩ@���̓��ς����
	// �����o�[�g�̗]���������̕\�ʂ̋P�x�͌����x�N�g���Ɩ@���x�N�g���̐����p���ƂƂ����Ƃ���cos�Ƃɔ�Ⴗ��
	return float4(brightness, brightness, brightness, 1) * diffuse;
}