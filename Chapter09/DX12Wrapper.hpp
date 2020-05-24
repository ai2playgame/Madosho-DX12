#pragma once
#define NOMINMAX
#include<d3d12.h>
#include<dxgi1_6.h>
#include<map>
#include<unordered_map>
#include<DirectXTex.h>
#include<wrl.h>
#include<string>
#include<functional>
#include <memory>

class DX12Wrapper
{
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

public:

	DX12Wrapper(HWND hwnd);
	~DX12Wrapper() = default;

	void update();
	void setScene();
	void beginDraw();
	void endDraw();

	// �e�N�X�`���p�X����K�v�ȃe�N�X�`���o�b�t�@�ւ̃|�C���^��Ԃ�
	// �e�N�X�`���t�@�C���p�X
	ComPtr<ID3D12Resource> getTextureByPath(const char* texpath);

	ComPtr<ID3D12Device> device();// �f�o�C�X
	ComPtr<ID3D12GraphicsCommandList> commandList();// �R�}���h���X�g
	ComPtr<IDXGISwapChain4> swapchain();// �X���b�v�`�F�C��


private:

	// ---------------------------------------------------------------- //
	//	private�����o�ϐ�
	// ---------------------------------------------------------------- //

	// �g�p����GPU�̃f�o�C�X��
	const std::wstring DEV_NAME = L"NVIDIA";

	// �E�B���h�E�T�C�Y
	SIZE m_winSize;

	// DXGI�܂��
	ComPtr<IDXGIFactory4> m_dxgiFactory = nullptr;// DXGI�C���^�[�t�F�C�X
	ComPtr<IDXGISwapChain4> m_swapchain = nullptr;// �X���b�v�`�F�C��

	// DirectX12�܂��
	ComPtr<ID3D12Device> m_dev = nullptr;// �f�o�C�X
	ComPtr<ID3D12CommandAllocator> m_cmdAllocator = nullptr;// �R�}���h�A���P�[�^
	ComPtr<ID3D12GraphicsCommandList> m_cmdList = nullptr;// �R�}���h���X�g
	ComPtr<ID3D12CommandQueue> m_cmdQueue = nullptr;// �R�}���h�L���[

	// �\���Ɋւ��o�b�t�@����
	ComPtr<ID3D12Resource> m_depthBuffer = nullptr;// �[�x�o�b�t�@
	std::vector<ID3D12Resource*> m_backBuffers;// �o�b�N�o�b�t�@(2�ȏ�c�X���b�v�`�F�C�����m��)
	ComPtr<ID3D12DescriptorHeap> m_rtvHeaps = nullptr;// �����_�[�^�[�Q�b�g�p�f�X�N���v�^�q�[�v
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;// �[�x�o�b�t�@�r���[�p�f�X�N���v�^�q�[�v

	std::unique_ptr<D3D12_VIEWPORT> m_viewport; // �r���[�|�[�g
	std::unique_ptr<D3D12_RECT> m_scissorrect; // �V�U�[��`

	// �V�[�����\������o�b�t�@��
	ComPtr<ID3D12Resource> m_sceneConstBuff = nullptr;

	struct SceneData {
		DirectX::XMMATRIX view;// �r���[�s��
		DirectX::XMMATRIX proj;// �v���W�F�N�V�����s��
		DirectX::XMFLOAT3 eye;// ���_���W
	};

	SceneData* m_mappedSceneData;
	ComPtr<ID3D12DescriptorHeap> m_sceneDescHeap = nullptr;

	// �t�F���X
	ComPtr<ID3D12Fence> m_fence = nullptr;
	UINT64 m_fenceVal = 0;

	// ���[�h�p�e�[�u��
	using LoadLambda_t = std::function<HRESULT(const std::wstring&,
		DirectX::TexMetadata*,
		DirectX::ScratchImage&)>;
	std::map < std::string, LoadLambda_t> m_loadLambdaTable;

	// �e�N�X�`���e�[�u��
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> m_textureTable;

	// ---------------------------------------------------------------- //
	//	private���\�b�h�錾				                            
	// ---------------------------------------------------------------- //

	// �ŏI�I�ȃ����_�[�^�[�Q�b�g�̐���
	HRESULT	createFinalRenderTargets();

	// �f�v�X�X�e���V���r���[�̐���
	HRESULT createDepthStencilView();

	// �X���b�v�`�F�C���̐���
	HRESULT createSwapChain(const HWND& hwnd);

	// DXGI�܂�菉����
	HRESULT initDXGIDevice();

	// �R�}���h�܂�菉����
	HRESULT initCommand();

	// �r���[�v���W�F�N�V�����p�r���[�̐���
	HRESULT createSceneView();

	// �e�N�X�`�����[�_�e�[�u���̍쐬
	void createTextureLoaderTable();

	// �e�N�X�`��������e�N�X�`���o�b�t�@�쐬�A���g���R�s�[
	ID3D12Resource* createTextureFromFile(const char* texpath);

};