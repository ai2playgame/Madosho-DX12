#pragma once
#include<Windows.h>
#include<tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXMath.h>
#include<vector>
#include<map>
#include<d3dcompiler.h>
#include<DirectXTex.h>
#include<d3dx12.h>
#include<wrl.h>
#include<memory>

// �O���錾
class DX12Wrapper;
class PMDRenderer;
class PMDActor;

// �V���O���g���N���X
class Application
{
	// ---------------------------------------------------------------- //
	//	�V���O���g���p�ݒ�
	// ---------------------------------------------------------------- //
private:
	// �R���X�g���N�^��private�Ő錾
	// �R�s�[�Ƒ�����֎~��
	Application() = default;
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

public:
	~Application() = default;
	// Application�̃V���O���g���C���X�^���X�𓾂�
	static Application& instance();

	// ---------------------------------------------------------------- //
	//	public���\�b�h
	// ---------------------------------------------------------------- //

	// ������
	bool init();

	// ���[�v�N��
	void run();

	// �㏈��
	void terminate();

	// �E�B���h�E�T�C�Y�擾
	SIZE getWindowSize() const;

	//�Q�[���p�E�B���h�E�̐���
	void createGameWindow(HWND &hwnd, WNDCLASSEX &windowClass);

	// ---------------------------------------------------------------- //
	//	public�t�B�[���h
	// ---------------------------------------------------------------- //

	//�E�B���h�E����
	static const unsigned int WIN_WIDTH;
	static const unsigned int WIN_HEIGHT;
	WNDCLASSEX m_windowClass;
	HWND m_hwnd;

	// DX12�`�惉�b�p
	std::shared_ptr<DX12Wrapper> m_dx12;
	
	// PMD�����_�����A�N�^�[
	std::shared_ptr<PMDRenderer> m_pmdRenderer;
	std::shared_ptr<PMDActor> m_pmdActor;
};
