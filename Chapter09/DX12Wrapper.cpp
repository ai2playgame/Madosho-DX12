#include "DX12Wrapper.hpp"
#include <cassert>
#include <d3dx12.h>
#include <algorithm>
#include <filesystem>
#include "Application.hpp"
#include "PathOperator.hpp"

// ---------------------------------------------------------------- //
//	pragma comment （リンクするライブラリを指定）
// ---------------------------------------------------------------- //
#pragma comment(lib, "DirectXTex.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ---------------------------------------------------------------- //
//	using 宣言
// ---------------------------------------------------------------- //
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

using namespace DirectX;

// ---------------------------------------------------------------- //
//	無名名前空間
// ---------------------------------------------------------------- //
namespace {
/// <summary>
/// デバッグレイヤを有効にする
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
//	public メソッド
// ---------------------------------------------------------------- //

DX12Wrapper::DX12Wrapper(HWND hwnd) {
#ifdef _DEBUG
	EnableDebugLayer();
#endif // _DEBUG

	auto& app = Application::instance();
	m_winSize = app.getWindowSize();

	// DX12初期化
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
	
	// テクスチャローダ初期化
	createTextureLoaderTable();

	// デプスステンシルバッファ作成
	if (FAILED(createDepthStencilView())) {
		assert(0);
		return;
	}

	// フェンス作成
	if (FAILED(m_dev->CreateFence(m_fenceVal, D3D12_FENCE_FLAG_NONE,
								  IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())))) {
		assert(0);
		return;
	}
}

void DX12Wrapper::update() {

}

void DX12Wrapper::setScene() {
	// 現在のシーン（ビュープロジェクション）をセット
	ID3D12DescriptorHeap* sceneheap[] = { m_sceneDescHeap.Get() };
	m_cmdList->SetDescriptorHeaps(1, sceneheap);
	m_cmdList->SetGraphicsRootDescriptorTable(0, m_sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
}

void DX12Wrapper::beginDraw() {
	// バックバッファのインデックスを取得
	auto bbIdx = m_swapchain->GetCurrentBackBufferIndex();

	// リソースバリアの設定
	// バックバッファをレンダーターゲット状態に遷移させる
	m_cmdList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[bbIdx],
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	
	// レンダーターゲットを指定
	auto rtvH = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// 深度を指定
	auto dsvH = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	m_cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);
	m_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// 画面クリア
	float clearColor[] = { 1.f, 0.f, 1.f, 1.f }; // 白色
	m_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	// ビューポート、シザー矩形の設定コマンド発行
	m_cmdList->RSSetViewports(1, m_viewport.get());
	m_cmdList->RSSetScissorRects(1, m_scissorrect.get());
}

void DX12Wrapper::endDraw() {
	auto bbIdx = m_swapchain->GetCurrentBackBufferIndex();

	// リソースバリアの設定
	// バックバッファをレンダーターゲット状態からPresent状態に遷移させる
	m_cmdList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[bbIdx],
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// コマンドリストを閉じる（必須！）
	m_cmdList->Close();

	// コマンドキューを介して描画コマンド実行
	ID3D12CommandList * cmdList[] = { m_cmdList.Get() };
	m_cmdQueue->ExecuteCommandLists(1, cmdList);

	// フェンスを使って描画コマンドが全て実行完了するまで待機
	m_cmdQueue->Signal(m_fence.Get(), ++m_fenceVal);
	if (m_fence->GetCompletedValue() < m_fenceVal) {
		auto fenceEvent = CreateEvent(nullptr, false, false, nullptr);
		m_fence->SetEventOnCompletion(m_fenceVal, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
		CloseHandle(fenceEvent);
	}

	m_cmdAllocator->Reset(); // コマンドキューをクリア
	m_cmdList->Reset(m_cmdAllocator.Get(), nullptr); // 次フレームのコマンドを受け入れる準備
}

// ---------------------------------------------------------------- //
//	private メソッド
// ---------------------------------------------------------------- //
HRESULT DX12Wrapper::createDepthStencilView() {
	// スワップチェーンの情報取得
	DXGI_SWAP_CHAIN_DESC1 scdesc{};
	auto result = m_swapchain->GetDesc1(&scdesc);
	if (FAILED(result)) {
		return result;
	}

	// 深度バッファ作成
	// CD3DX12_RESOURCE_DESC::Tex2D()でも作成可能だが
	// 各パラメータの正体が分かりにくくなるため、今回は直接構造体を構築
	D3D12_RESOURCE_DESC resdesc{};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resdesc.DepthOrArraySize = 1;
	resdesc.Width = scdesc.Width;
	resdesc.Height = scdesc.Height;
	resdesc.Format = DXGI_FORMAT_D32_FLOAT;
	resdesc.SampleDesc.Count = 1;	//アンチエイリアスを行わない
	resdesc.SampleDesc.Quality = 0;
	resdesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	// デプスステンシルとして利用
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resdesc.MipLevels = 1;
	resdesc.Alignment = 0;

	// デプスステンシル用のヒーププロパティ
	auto depthHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// 深度の初期化値
    // 深度は1.0f、ステンシルは0（今回は無効だが）で初期化
	CD3DX12_CLEAR_VALUE depthClearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

	// 深度ステンシルバッファとしてのGPUリソースを確保
	result = m_dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,	// 深度書き込みに利用する
		&depthClearValue,	// 深度ステンシルの初期値
		IID_PPV_ARGS(m_depthBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		assert(0);
		return result;
	}

	// 深度用のディスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.NumDescriptors = 1; // 深度ビュー一つのみ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;	// DepthStencilViewとして扱う
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// DSV用のディスクリプタヒープを作成
	result = m_dev->CreateDescriptorHeap(&dsvHeapDesc, 
										 IID_PPV_ARGS(m_dsvHeap.ReleaseAndGetAddressOf()));

	// 深度ステンシルビューを作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;	// デプス値に32bit使用。
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;	// 2Dテクスチャ
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE; // フラグは特になし
	m_dev->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

// ---------------------------------------------------------------- //
//	private メソッド
// ---------------------------------------------------------------- //
HRESULT DX12Wrapper::initDXGIDevice() {
	UINT flagDXGI = 0;
	flagDXGI |= DXGI_CREATE_FACTORY_DEBUG;
	auto result = CreateDXGIFactory2(flagDXGI,
									 IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	// feature level列挙
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	// NVIDIAのアダプタを見つける
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

	// Direct3Dデバイスの初期化
	result = S_FALSE;
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (SUCCEEDED(D3D12CreateDevice(usedAdapter, l, IID_PPV_ARGS(m_dev.ReleaseAndGetAddressOf()) ))) {
			// featureLevel = l; // いらない
			result = S_OK;
			break;
		}
	}
	return result;
}

// コマンドアロケータ・コマンドリスト・コマンドキューを生成
HRESULT DX12Wrapper::initCommand() {
	// コマンドアロケータを生成
	auto result = m_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_cmdAllocator.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}
	// コマンドリストを生成
	result = m_dev->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_cmdAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(m_cmdList.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	// コマンドキュー生成
	D3D12_COMMAND_QUEUE_DESC cmdQueDesc{};
	cmdQueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // タイムアウトなし
	cmdQueDesc.NodeMask = 0;	// アダプタを1つしか使わないときは0で良い
	cmdQueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // priorityは特に指定なし
	cmdQueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;	// コマンドリストと揃える

	result = m_dev->CreateCommandQueue(&cmdQueDesc, IID_PPV_ARGS(m_cmdQueue.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	return result;
}

HRESULT DX12Wrapper::createSwapChain(const HWND& hwnd) {
	RECT rc{};
	::GetWindowRect(hwnd, &rc);

	// スワップチェイン設定構造体を生成
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
	swapchainDesc.Width = m_winSize.cx;
	swapchainDesc.Height = m_winSize.cy;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchainDesc.BufferCount = 2;	// ダブルバッファ
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	
	// スワップチェイン生成
	auto result = m_dxgiFactory->CreateSwapChainForHwnd(m_cmdQueue.Get(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)m_swapchain.ReleaseAndGetAddressOf());
	
	assert(SUCCEEDED(result));
	return result;
}

// シーン投影に用いるビューを作成
HRESULT DX12Wrapper::createSceneView() {
	DXGI_SWAP_CHAIN_DESC1 scdesc{};
	auto result = m_swapchain->GetDesc1(&scdesc);

	// 定数バッファ（ワールド・ビュー・プロジェクション行列を保持する）作成
	// 定数バッファは256アライメントで作成
	result = m_dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneData) + 0xff) & ~0xff), 
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_sceneConstBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result)) {
		assert(0);
		return result;
	}
    // シーンデータをマップする先を示すポインタ
	m_mappedSceneData = nullptr; 
	result = m_sceneConstBuff->Map(0, nullptr, (void**)&m_mappedSceneData);
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	XMFLOAT3 eye(0, 15, -15); // 視点座標
	XMFLOAT3 target(0, 15, 0); // 注視点
	XMFLOAT3 up(0, 1, 0); // 上行列
	
    // ---------------------------------------------------------------- //
    //  行列計算
    // ---------------------------------------------------------------- //

	// ビュー行列を計算
	m_mappedSceneData->view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	// プロジェクション行列を計算
	m_mappedSceneData->proj = XMMatrixPerspectiveFovLH(
		XM_PIDIV4, // 画角45度
		static_cast<float>(scdesc.Width) / static_cast<float>(scdesc.Height), // アスペクト比
		0.1f, // 視錐体のnear面
		1000.f // 視錐体のfar面
	);
	m_mappedSceneData->eye = eye;

    // ---------------------------------------------------------------- //
    //  ディスクリプタヒープ・CBV作成
    // ---------------------------------------------------------------- //

	// 定数バッファ用のディスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc{};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // シェーダから見える
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;	// ディスクリプタヒープ種別
	
	result = m_dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(m_sceneDescHeap.ReleaseAndGetAddressOf()));

	// ディスクリプタヒープの先頭ハンドルを取得
	auto heapHandle = m_sceneDescHeap->GetCPUDescriptorHandleForHeapStart();
	
	// 定数バッファビュー作成作成
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvdesc{};
	cbvdesc.BufferLocation = m_sceneConstBuff->GetGPUVirtualAddress();
	cbvdesc.SizeInBytes = m_sceneConstBuff->GetDesc().Width;
	m_dev->CreateConstantBufferView(&cbvdesc, heapHandle);

	return result;
}

HRESULT DX12Wrapper::createFinalRenderTargets() {
	DXGI_SWAP_CHAIN_DESC1 desc{};
	auto result = m_swapchain->GetDesc1(&desc);

	// RTV用のディスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // ダブルバッファなので2つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;	// 特に指定なし
	result = m_dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvHeaps.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}
	
	// スワップチェイン用のRTVをスワップチェインのバッファ数分作成
	DXGI_SWAP_CHAIN_DESC swcDesc{};
	result = m_swapchain->GetDesc(&swcDesc);
	m_backBuffers.resize(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	
	// SRGBのレンダーターゲットビューの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (int i = 0; i < swcDesc.BufferCount; ++i) {
		result = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
		assert(SUCCEEDED(result));
		rtvDesc.Format = m_backBuffers[i]->GetDesc().Format;
		// 実際にRTVを作成
		m_dev->CreateRenderTargetView(m_backBuffers[i], &rtvDesc, handle);
		handle.ptr += m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	
	// ビューポートとシザー矩形を設定
	m_viewport.reset(new CD3DX12_VIEWPORT(m_backBuffers[0]));
	m_scissorrect.reset(new CD3DX12_RECT(0, 0, desc.Width, desc.Height));
	
	return result;
}

ComPtr<ID3D12Resource> DX12Wrapper::getTextureByPath(const char* texpath) {
	auto it = m_textureTable.find(texpath);
	if (it != m_textureTable.end()) {
		// テーブル内に指定したテクスチャがあればロードするのではなく、マッピング済のリソースを返す
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
	// テクスチャロード
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

	auto img = scrachImg.GetImage(0, 0, 0);	// 生の画像データ取得

	// WriteToSubresourceで転送するためのヒープ設定
	// GPU上のリソースにテクスチャをコピーする
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
											   D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(texMeta.format,
												texMeta.width, texMeta.height,
												texMeta.arraySize, texMeta.mipLevels);

	// テクスチャ用のGPUリソースを作成
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
	// テクスチャデータをGPU上のメモリにコピー
	result = texBuff->WriteToSubresource(0, nullptr, img->pixels, img->rowPitch, img->slicePitch);
	if (FAILED(result)) {
		assert(0);
		return nullptr;
	}
	return texBuff;
}
