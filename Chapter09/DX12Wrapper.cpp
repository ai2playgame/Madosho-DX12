#include "DX12Wrapper.hpp"
#include <cassert>
#include <d3dx12.h>
#include <algorithm>
#include <filesystem>
#include "Application.hpp"
#include "PathOperator.hpp"

// ---------------------------------------------------------------- //
//	pragma comment �i�����N���郉�C�u�������w��j
// ---------------------------------------------------------------- //
#pragma comment(lib, "DirectXTex.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ---------------------------------------------------------------- //
//	using �錾
// ---------------------------------------------------------------- //
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

using namespace DirectX;

// ---------------------------------------------------------------- //
//	�������O���
// ---------------------------------------------------------------- //
namespace {
/// <summary>
/// �f�o�b�O���C����L���ɂ���
/// </summary>
void EnableDebugLayer() {
	ComPtr<ID3D12Debug> debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	debugLayer->EnableDebugLayer();
}
}

// ---------------------------------------------------------------- //
//	public Getter
// ---------------------------------------------------------------- //
ComPtr<ID3D12Device> DX12Wrapper::device() {
	return m_dev;
}

ComPtr<ID3D12GraphicsCommandList> DX12Wrapper::commandList() {
	return m_cmdList;
}


ComPtr<IDXGISwapChain4> DX12Wrapper::swapchain() {
	return m_swapchain;
}

// ---------------------------------------------------------------- //
//	public ���\�b�h
// ---------------------------------------------------------------- //

DX12Wrapper::DX12Wrapper(HWND hwnd) {
#ifdef _DEBUG
	EnableDebugLayer();
#endif // _DEBUG

	auto& app = Application::instance();
	m_winSize = app.getWindowSize();

	// DX12������
	if (FAILED(initDXGIDevice())) {
		assert(0);
		return;
	}
	if (FAILED(initCommand())) {
		assert(0);
		return;
	}
	if (FAILED(createSwapChain(hwnd))) {
		assert(0);
		return;
	}
	if (FAILED(createFinalRenderTargets())) {
		assert(0);
		return;
	}
	if (FAILED(createSceneView())) {
		assert(0);
		return;
	}
	
	// �e�N�X�`�����[�_������
	createTextureLoaderTable();

	// �f�v�X�X�e���V���o�b�t�@�쐬
	if (FAILED(createDepthStencilView())) {
		assert(0);
		return;
	}

	// �t�F���X�쐬
	if (FAILED(m_dev->CreateFence(m_fenceVal, D3D12_FENCE_FLAG_NONE,
								  IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())))) {
		assert(0);
		return;
	}
}

void DX12Wrapper::update() {

}

void DX12Wrapper::setScene() {
	// ���݂̃V�[���i�r���[�v���W�F�N�V�����j���Z�b�g
	ID3D12DescriptorHeap* sceneheap[] = { m_sceneDescHeap.Get() };
	m_cmdList->SetDescriptorHeaps(1, sceneheap);
	m_cmdList->SetGraphicsRootDescriptorTable(0, m_sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
}

void DX12Wrapper::beginDraw() {
	// �o�b�N�o�b�t�@�̃C���f�b�N�X���擾
	auto bbIdx = m_swapchain->GetCurrentBackBufferIndex();

	// ���\�[�X�o���A�̐ݒ�
	// �o�b�N�o�b�t�@�������_�[�^�[�Q�b�g��ԂɑJ�ڂ�����
	m_cmdList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[bbIdx],
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	
	// �����_�[�^�[�Q�b�g���w��
	auto rtvH = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// �[�x���w��
	auto dsvH = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	m_cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);
	m_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// ��ʃN���A
	float clearColor[] = { 1.f, 0.f, 1.f, 1.f }; // ���F
	m_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	// �r���[�|�[�g�A�V�U�[��`�̐ݒ�R�}���h���s
	m_cmdList->RSSetViewports(1, m_viewport.get());
	m_cmdList->RSSetScissorRects(1, m_scissorrect.get());
}

void DX12Wrapper::endDraw() {
	auto bbIdx = m_swapchain->GetCurrentBackBufferIndex();

	// ���\�[�X�o���A�̐ݒ�
	// �o�b�N�o�b�t�@�������_�[�^�[�Q�b�g��Ԃ���Present��ԂɑJ�ڂ�����
	m_cmdList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[bbIdx],
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// �R�}���h���X�g�����i�K�{�I�j
	m_cmdList->Close();

	// �R�}���h�L���[����ĕ`��R�}���h���s
	ID3D12CommandList * cmdList[] = { m_cmdList.Get() };
	m_cmdQueue->ExecuteCommandLists(1, cmdList);

	// �t�F���X���g���ĕ`��R�}���h���S�Ď��s��������܂őҋ@
	m_cmdQueue->Signal(m_fence.Get(), ++m_fenceVal);
	if (m_fence->GetCompletedValue() < m_fenceVal) {
		auto fenceEvent = CreateEvent(nullptr, false, false, nullptr);
		m_fence->SetEventOnCompletion(m_fenceVal, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
		CloseHandle(fenceEvent);
	}

	m_cmdAllocator->Reset(); // �R�}���h�L���[���N���A
	m_cmdList->Reset(m_cmdAllocator.Get(), nullptr); // ���t���[���̃R�}���h���󂯓���鏀��
}

// ---------------------------------------------------------------- //
//	private ���\�b�h
// ---------------------------------------------------------------- //
HRESULT DX12Wrapper::createDepthStencilView() {
	// �X���b�v�`�F�[���̏��擾
	DXGI_SWAP_CHAIN_DESC1 scdesc{};
	auto result = m_swapchain->GetDesc1(&scdesc);
	if (FAILED(result)) {
		return result;
	}

	// �[�x�o�b�t�@�쐬
	// �[�x�o�b�t�@Desc�쐬
	// CD3DX12_RESOURCE_DESC::Tex2D()�ł��쐬�\����
	// �e�p�����[�^�̐��̂�������ɂ����Ȃ邽�߁A����͒��ڍ\���̂��\�z
	D3D12_RESOURCE_DESC resdesc{};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resdesc.DepthOrArraySize = 1;
	resdesc.Width = scdesc.Width;
	resdesc.Height = scdesc.Height;
	resdesc.Format = DXGI_FORMAT_D32_FLOAT;
	resdesc.SampleDesc.Count = 1;	//�A���`�G�C���A�X���s��Ȃ�
	resdesc.SampleDesc.Quality = 0;
	resdesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	// �f�v�X�X�e���V���Ƃ��ė��p
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resdesc.MipLevels = 1;
	resdesc.Alignment = 0;

	// �f�v�X�X�e���V���p�̃q�[�v�v���p�e�B
	auto depthHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// �[�x�̏������l
    // �[�x��1.0f�A�X�e���V����0�i����͖��������j�ŏ�����
	CD3DX12_CLEAR_VALUE depthClearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

	// �[�x�X�e���V���o�b�t�@�Ƃ��Ă�GPU���\�[�X���m��
	result = m_dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,	// �[�x�������݂ɗ��p����
		&depthClearValue,	// �[�x�X�e���V���̏����l
		IID_PPV_ARGS(m_depthBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		assert(0);
		return result;
	}

	// �[�x�p�̃f�B�X�N���v�^�q�[�v�̐ݒ�
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.NumDescriptors = 1; // �[�x�r���[��̂�
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;	// DepthStencilView�Ƃ��Ĉ���
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// �[�x�p�̃f�B�X�N���v�^�q�[�v���쐬
	result = m_dev->CreateDescriptorHeap(&dsvHeapDesc, 
										 IID_PPV_ARGS(m_dsvHeap.ReleaseAndGetAddressOf()));

	// �[�x�X�e���V���r���[���쐬
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;	// �f�v�X�l��32bit�g�p�B
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;	// 2D�e�N�X�`��
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE; // �t���O�͓��ɂȂ�
	m_dev->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

// ---------------------------------------------------------------- //
//	private ���\�b�h
// ---------------------------------------------------------------- //

ComPtr<ID3D12Resource> DX12Wrapper::getTextureByPath(const char* texpath) {
	auto it = m_textureTable.find(texpath);
	if (it != m_textureTable.end()) {
		// �e�[�u�����Ɏw�肵���e�N�X�`��������΃��[�h����̂ł͂Ȃ��A�}�b�s���O�ς̃��\�[�X��Ԃ�
		return m_textureTable[texpath];
	} else {
		return ComPtr<ID3D12Resource>(createTextureFromFile(texpath));
	}
}

void DX12Wrapper::createTextureLoaderTable() {
	auto WICLoader = [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img) -> HRESULT {
		return DirectX::LoadFromWICFile(path.c_str(), 0, meta, img);
	};
	auto TGALoader = [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img) -> HRESULT {
		return DirectX::LoadFromTGAFile(path.c_str(), meta, img);
	};
	auto DDSLoader = [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img) -> HRESULT {
		return DirectX::LoadFromDDSFile(path.c_str(), 0, meta, img);
	};

	// WIC File
	m_loadLambdaTable["sph"] = WICLoader;
	m_loadLambdaTable["spa"] = WICLoader;
	m_loadLambdaTable["bmp"] = WICLoader;
	m_loadLambdaTable["pbg"] = WICLoader;
	m_loadLambdaTable["jpg"] = WICLoader;

	// TGA File
	m_loadLambdaTable["tga"] = TGALoader;

	// DDS File
	m_loadLambdaTable["dds"] = DDSLoader;
}

ID3D12Resource* DX12Wrapper::createTextureFromFile(const char* texPath) {
	// �e�N�X�`�����[�h
	DirectX::TexMetadata texMeta{};
	DirectX::ScratchImage scrachImg{};
	std::string texPathStr{ texPath };
	auto texPathWStr = PathOperator::toWString(texPathStr);
	auto ext = PathOperator::getExtension(texPathStr);
	auto result = m_loadLambdaTable[ext](texPathWStr, &texMeta, scrachImg);
	if (FAILED(result)) {
		assert(0);
		return nullptr;
	}

	auto img = scrachImg.GetImage(0, 0, 0);	// ���̉摜�f�[�^�擾

	// WriteToSubresource�œ]�����邽�߂̃q�[�v�ݒ�
	// GPU��̃��\�[�X�Ƀe�N�X�`�����R�s�[����
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
											   D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(texMeta.format,
												texMeta.width, texMeta.height,
												texMeta.arraySize, texMeta.mipLevels);

	// �e�N�X�`���p��GPU���\�[�X���쐬
	ID3D12Resource* texBuff = nullptr;
	result = m_dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);
	if (FAILED(result)) {
		assert(0);
		return nullptr;
	}
	// �e�N�X�`���f�[�^��GPU��̃������ɃR�s�[
	result = texBuff->WriteToSubresource(0, nullptr, img->pixels, img->rowPitch, img->slicePitch);
	if (FAILED(result)) {
		assert(0);
		return nullptr;
	}
	return texBuff;
}

HRESULT DX12Wrapper::initDXGIDevice() {
	UINT flagDXGI = 0;
	flagDXGI |= DXGI_CREATE_FACTORY_DEBUG;
	auto result = CreateDXGIFactory2(flagDXGI,
									 IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	// feature level��
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	// NVIDIA�̃A�_�v�^��������
	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* usedAdapter = nullptr;
	for (int i = 0; m_dxgiFactory->EnumAdapters(i, &usedAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		adapters.push_back(usedAdapter);
	}
	for (auto adap : adapters) {
		DXGI_ADAPTER_DESC adesc{};
		adap->GetDesc(&adesc);
		std::wstring wstrDesc = adesc.Description;
		if (wstrDesc.find(DEV_NAME) != std::string::npos) {
			usedAdapter = adap;
			break;
		}
	}

	// Direct3D�f�o�C�X�̏�����
	result = S_FALSE;
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (SUCCEEDED(D3D12CreateDevice(usedAdapter, l, IID_PPV_ARGS(m_dev.ReleaseAndGetAddressOf()) ))) {
			// featureLevel = l; // ����Ȃ�
			result = S_OK;
			break;
		}
	}
	return result;
}

HRESULT DX12Wrapper::createSwapChain(const HWND& hwnd) {
	RECT rc{};
	::GetWindowRect(hwnd, &rc);

	// �X���b�v�`�F�C���ݒ�\���̂𐶐�
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
	swapchainDesc.Width = m_winSize.cx;
	swapchainDesc.Height = m_winSize.cy;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchainDesc.BufferCount = 2;	// �_�u���o�b�t�@
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	
	// �X���b�v�`�F�C������
	auto result = m_dxgiFactory->CreateSwapChainForHwnd(m_cmdQueue.Get(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)m_swapchain.ReleaseAndGetAddressOf());
	
	assert(SUCCEEDED(result));
	return result;
}

// �R�}���h�A���P�[�^�E�R�}���h���X�g�E�R�}���h�L���[�𐶐�
HRESULT DX12Wrapper::initCommand() {
	// �R�}���h�A���P�[�^�𐶐�
	auto result = m_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_cmdAllocator.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}
	// �R�}���h���X�g�𐶐�
	result = m_dev->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_cmdAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(m_cmdList.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	// �R�}���h�L���[����
	D3D12_COMMAND_QUEUE_DESC cmdQueDesc{};
	cmdQueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // �^�C���A�E�g�Ȃ�
	cmdQueDesc.NodeMask = 0;	// �A�_�v�^��1�����g��Ȃ��Ƃ���0�ŗǂ�
	cmdQueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // priority�͓��Ɏw��Ȃ�
	cmdQueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;	// �R�}���h���X�g�Ƒ�����

	result = m_dev->CreateCommandQueue(&cmdQueDesc, IID_PPV_ARGS(m_cmdQueue.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	return result;
}

// �V�[�����e�p�̃r���[���쐬
HRESULT DX12Wrapper::createSceneView() {
	DXGI_SWAP_CHAIN_DESC1 scdesc{};
	auto result = m_swapchain->GetDesc1(&scdesc);

	// �萔�o�b�t�@�쐬
	result = m_dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneData) + 0xff) & ~0xff), // �萔�o�b�t�@��256�A���C�����g
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_sceneConstBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	m_mappedSceneData = nullptr; // �V�[���f�[�^���}�b�v�����������|�C���^
	result = m_sceneConstBuff->Map(0, nullptr, (void**)&m_mappedSceneData);
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	XMFLOAT3 eye(0, 15, -15); // ���_���W
	XMFLOAT3 target(0, 15, 0); // �����_
	XMFLOAT3 up(0, 1, 0); // ��s��

	// �r���[�s����v�Z
	m_mappedSceneData->view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	// �v���W�F�N�V�����s����v�Z
	m_mappedSceneData->proj = XMMatrixPerspectiveFovLH(
		XM_PIDIV4, // ��p45�x
		static_cast<float>(scdesc.Width) / static_cast<float>(scdesc.Height), // �A�X�y�N�g��
		0.1f, // �����̂�near��
		1000.f // �����̂�far��
	);
	m_mappedSceneData->eye = eye;
	
	// �萔�o�b�t�@�p�̃f�B�X�N���v�^�q�[�v
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc{};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // �V�F�[�_���猩����
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;	// �f�B�X�N���v�^�q�[�v���
	
	result = m_dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(m_sceneDescHeap.ReleaseAndGetAddressOf()));

	// �f�B�X�N���v�^�̐擪�n���h�����擾
	auto heapHandle = m_sceneDescHeap->GetCPUDescriptorHandleForHeapStart();
	
	// �萔�o�b�t�@�r���[�쐬�쐬
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvdesc{};
	cbvdesc.BufferLocation = m_sceneConstBuff->GetGPUVirtualAddress();
	cbvdesc.SizeInBytes = m_sceneConstBuff->GetDesc().Width;
	m_dev->CreateConstantBufferView(&cbvdesc, heapHandle);

	return result;
}

HRESULT DX12Wrapper::createFinalRenderTargets() {
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = m_swapchain->GetDesc1(&desc);

	// RTV�p�̃f�B�X�N���v�^�q�[�v�쐬
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // �_�u���o�b�t�@�Ȃ̂�2��
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;	// ���Ɏw��Ȃ�
	result = m_dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvHeaps.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}
	
	// �X���b�v�`�F�C���p��RTV���X���b�v�`�F�C���̃o�b�t�@�����쐬
	DXGI_SWAP_CHAIN_DESC swcDesc{};
	result = m_swapchain->GetDesc(&swcDesc);
	m_backBuffers.resize(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	
	// SRGB�̃����_�[�^�[�Q�b�g�r���[�̐ݒ�
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (int i = 0; i < swcDesc.BufferCount; ++i) {
		result = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
		assert(SUCCEEDED(result));
		rtvDesc.Format = m_backBuffers[i]->GetDesc().Format;
		// ���ۂ�RTV���쐬
		m_dev->CreateRenderTargetView(m_backBuffers[i], &rtvDesc, handle);
		handle.ptr += m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	
	// �r���[�|�[�g�ƃV�U�[��`��ݒ�
	m_viewport.reset(new CD3DX12_VIEWPORT(m_backBuffers[0]));
	m_scissorrect.reset(new CD3DX12_RECT(0, 0, desc.Width, desc.Height));
	
	return result;
}

