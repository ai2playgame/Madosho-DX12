#include "DX12Wrapper.hpp"
#include <cassert>
#include <d3dx12.h>
#include <algorithm>
#include <filesystem>
#include "Application.hpp"
#include "PathOperator.hpp"
#include "PMDRenderer.hpp"
#include "ErrHandler.hpp"

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

ComPtr<ID3D12Resource> DX12Wrapper::whiteTexture()
{
	return m_whiteTex;
}

ComPtr<ID3D12Resource> DX12Wrapper::blackTexture()
{
	return m_blackTex;
}

ComPtr<ID3D12Resource> DX12Wrapper::gradTexture()
{
	return m_gradTex;
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

	// 黒白グラデーションテクスチャ一式作成
	if (!createWhiteTexture()) {
		assert(0);
		return;
	}
	if (!createBlackTexture()) {
		assert(0);
		return;
	}
	if (!createGrayGradationTexture()) {
		assert(0);
		return;
	}

	// マルチパスレンダリング用の描画パス
	if (FAILED(createPeraResourceAndView())) {
		assert(0);
		return;
	}
	if (FAILED(createPeraVertex())) {
		assert(0);
		return;
	}
	if (FAILED(createPeraPipeline())) {
		assert(0);
		return;
	}
}

void DX12Wrapper::update() {

}

/*
void DX12Wrapper::setScene() {
	// 現在のシーン（ビュープロジェクション）をセット
	ID3D12DescriptorHeap* sceneheap[] = { m_sceneDescHeap.Get() };
	m_cmdList->SetDescriptorHeaps(1, sceneheap);
	m_cmdList->SetGraphicsRootDescriptorTable(0, m_sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
}
*/

void DX12Wrapper::clear() {
	// バックバッファのインデックスを取得
	size_t bbIdx = m_swapchain->GetCurrentBackBufferIndex();

	// リソースバリアの設定
	// バックバッファをレンダーターゲット状態に遷移させる
	m_cmdList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[bbIdx],
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	
	// レンダーターゲットを指定
	auto rtvHPtr = m_rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvHPtr.ptr += bbIdx * m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	m_cmdList->OMSetRenderTargets(1, &rtvHPtr, false, nullptr);

	// 画面クリア
	float clearColor[] = { 0.2f, 0.5f, 0.5f, 1.0f };
	m_cmdList->ClearRenderTargetView(rtvHPtr, clearColor, 0, nullptr);
}

void DX12Wrapper::flip() {
	auto bbIdx = m_swapchain->GetCurrentBackBufferIndex();

	// バックバッファをレンダーターゲット状態からPresent状態に遷移させる
	m_cmdList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[bbIdx],
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT));

	// コマンドリストを閉じる（必須！）
	m_cmdList->Close();

	// コマンドキューを介して描画コマンド実行
	ID3D12CommandList * cmdList[] = { m_cmdList.Get() };
	m_cmdQueue->ExecuteCommandLists(1, cmdList);

	// フェンスを使って描画コマンドが全て実行完了するまで待機
	// サンプル実装のWaitForCommandQueue()に相当する
	m_cmdQueue->Signal(m_fence.Get(), ++m_fenceVal);
	if (m_fence->GetCompletedValue() < m_fenceVal) {
		auto fenceEvent = CreateEvent(nullptr, false, false, nullptr);
		m_fence->SetEventOnCompletion(m_fenceVal, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
		CloseHandle(fenceEvent);
	}
	
	// コマンドキューをクリア
	m_cmdAllocator->Reset();
    // 次フレームのコマンドを受け入れる準備
	m_cmdList->Reset(m_cmdAllocator.Get(), nullptr); 
	
	// バッファを表示 
	m_swapchain->Present(0, 0);
}

void DX12Wrapper::draw(std::shared_ptr<PMDRenderer> renderer)
{
	// ビューポート・シザー矩形設定
	m_cmdList->RSSetViewports(1, m_viewport.get());
	m_cmdList->RSSetScissorRects(1, m_scissorrect.get());

	// ペラポリ用のルートシグネチャ設定
	m_cmdList->SetGraphicsRootSignature(m_peraRS.Get());

	// ペラポリ用のSRVヒープをセット
	m_cmdList->SetDescriptorHeaps(1, m_peraSRVHeap.GetAddressOf());

	// s0レジスタとヒープを関連付ける
	auto handle = m_peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	m_cmdList->SetGraphicsRootDescriptorTable(0, handle);

	// ペラポリ用のパイプラインステートを割り当て
	m_cmdList->SetPipelineState(m_peraPipeline.Get());
	
	// ペラポリ描画
	m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_cmdList->IASetVertexBuffers(0, 1, &m_peraVBView);
	m_cmdList->DrawInstanced(4, 1, 0, 0);
}

HRESULT DX12Wrapper::preDrawToPera1()
{
	// ペラポリ用のリソースをRenderTarget状態に
    m_cmdList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(m_peraResource.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET));

    auto rtvHeapPointer = m_peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvheapPointer = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	m_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, &dsvheapPointer);

	// レンダーターゲットと深度バッファを初期化
	float clsClr[4] = { 0.0f, 0.5f, 0.5f, 1.0f };
	m_cmdList->ClearRenderTargetView(rtvHeapPointer, clsClr, 0, nullptr);
	m_cmdList->ClearDepthStencilView(dsvheapPointer,
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    return S_OK;
}

void DX12Wrapper::postDrawToPera1()
{
    m_cmdList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(m_peraResource.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
}

void DX12Wrapper::drawToPera1(std::shared_ptr<PMDRenderer> renderer)
{
	// ビュー・プロジェクション行列と視点座標のディスクリプタヒープをセット
	ID3D12DescriptorHeap* heaps[] = { m_sceneDescHeap.Get() };
	// TODO: 要らない？
	// heaps[0] = m_sceneDescHeap.Get();
	m_cmdList->SetDescriptorHeaps(1, heaps);
	auto sceneHandle = m_sceneDescHeap->GetGPUDescriptorHandleForHeapStart();
	// ディスクリプタテーブルを設定
	m_cmdList->SetGraphicsRootDescriptorTable(1, sceneHandle);

	m_cmdList->RSSetViewports(1, m_viewport.get());
	m_cmdList->RSSetScissorRects(1, m_scissorrect.get());
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

	XMFLOAT3 eye(0, 15, -30); // 視点座標
	XMFLOAT3 target(0, 10, 0); // 注視点
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
    //  CBV用ディスクリプタヒープ・CBV作成
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
	cbvdesc.SizeInBytes = static_cast<UINT>(m_sceneConstBuff->GetDesc().Width);
	m_dev->CreateConstantBufferView(&cbvdesc, heapHandle);

	return result;
}

HRESULT DX12Wrapper::createPeraResourceAndView()
{
	// 既に作成済みのバックバッファのDescを再利用
	auto resDesc = m_backBuffers[0]->GetDesc();
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	// ペラポリゴンの色指定
	float clearColor[4] = { 0.5, 0.5, 0.5, 1.0 };
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clearColor);
	// 実際にペラポリゴンのGPUリソースを作成
	auto result = m_dev->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue, IID_PPV_ARGS(m_peraResource.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	// ペラポリ用のRTV作成
	
	// まずRTV用ヒープ作成
	auto heapDesc = m_rtvHeaps->GetDesc();
	heapDesc.NumDescriptors = 1;
	result = m_dev->CreateDescriptorHeap(&heapDesc,
		IID_PPV_ARGS(m_peraRTVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}
	// 実際にRTVを作成する
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	m_dev->CreateRenderTargetView(m_peraResource.Get(),
		&rtvDesc, m_peraRTVHeap->GetCPUDescriptorHandleForHeapStart());

	// ペラポリ用のSRV作成

	// まずSRV用ヒープ作成
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	result = m_dev->CreateDescriptorHeap(&heapDesc,
		IID_PPV_ARGS(m_peraSRVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}
	
	// 実際にSRVを作成する
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = rtvDesc.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	m_dev->CreateShaderResourceView(m_peraResource.Get(),
		&srvDesc, m_peraSRVHeap->GetCPUDescriptorHandleForHeapStart());


	return S_OK;
}

HRESULT DX12Wrapper::createPeraVertex()
{
	struct PeraVertex {
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};
	PeraVertex pv[4] = { {{-1,-1,0.1f},{0,1}},
						{{-1,1,0.1f},{0,0}},
						{{1,-1,0.1f},{1,1}},
						{{1,1,0.1f},{1,0}} };
	// 頂点バッファを作成
	auto result = m_dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(pv)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_peraVB.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	// 頂点バッファをマップして書き込み
	PeraVertex* mappedPera = nullptr;
	m_peraVB->Map(0, nullptr, (void**)&mappedPera);
	std::copy(std::begin(pv), std::end(pv), mappedPera);
	m_peraVB->Unmap(0, nullptr);
	
	// 頂点バッファビュー作成
	m_peraVBView.BufferLocation = m_peraVB->GetGPUVirtualAddress();
	m_peraVBView.SizeInBytes = sizeof(pv);
	m_peraVBView.StrideInBytes = sizeof(PeraVertex);

	return S_OK;
}

HRESULT DX12Wrapper::createPeraPipeline()
{
    HRESULT result;
    ComPtr<ID3DBlob> errBlob;

	// ---------------------------------------------------------------- //
    //  ディスクリプタレンジ設定
    // ---------------------------------------------------------------- //
	D3D12_DESCRIPTOR_RANGE range{};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.BaseShaderRegister = 0;
	range.NumDescriptors = 1;
	
	// ---------------------------------------------------------------- //
    //  ルートパラメータ設定
    // ---------------------------------------------------------------- //
	D3D12_ROOT_PARAMETER rootParam{};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam.DescriptorTable.pDescriptorRanges = &range;
	rootParam.DescriptorTable.NumDescriptorRanges = 1;

	// ---------------------------------------------------------------- //
    //  サンプラー設定
    // ---------------------------------------------------------------- //
	D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0);

	// ---------------------------------------------------------------- //
    //  ルートシグネチャ設定
    // ---------------------------------------------------------------- //
	D3D12_ROOT_SIGNATURE_DESC rsDesc;
	rsDesc.NumParameters = 1;
	rsDesc.pParameters = &rootParam;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.pStaticSamplers = &sampler;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	
	ComPtr<ID3DBlob> rsBlob;
	result = D3D12SerializeRootSignature(&rsDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		rsBlob.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (!ErrHandler::checkResult(result, errBlob.Get())) {
		assert(0);
		return result;
	}
	result = m_dev->CreateRootSignature(0,
		rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(),
		IID_PPV_ARGS((m_peraRS.ReleaseAndGetAddressOf())));
	if (FAILED(result)) {
		assert(0);
		return result;
	}
	
	// ---------------------------------------------------------------- //
    //  シェーダコンパイル
    // ---------------------------------------------------------------- //
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    result = D3DCompileFromFile(
        L"PeraVertexShader.hlsl", nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "PeraVS", "vs_5_0", 0, 0,
        vsBlob.ReleaseAndGetAddressOf(),
        errBlob.ReleaseAndGetAddressOf());
    if (!ErrHandler::checkResult(result, errBlob.Get())) {
        assert(0);
        return result;
    }
	result = D3DCompileFromFile(
		L"PeraPixelShader.hlsl", nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PeraPS", "ps_5_0", 0, 0,
		psBlob.ReleaseAndGetAddressOf(),
		errBlob.ReleaseAndGetAddressOf()
	);
	if (!ErrHandler::checkResult(result, errBlob.Get())) {
		assert(0);
		return result;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc{};
	gpsDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	gpsDesc.DepthStencilState.DepthEnable = false;
	gpsDesc.DepthStencilState.StencilEnable = false;

    // ペラポリゴンの頂点シェーダに流す頂点レイアウト
    D3D12_INPUT_ELEMENT_DESC layout[2] = {
        { "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
        { "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
    };
	gpsDesc.InputLayout.NumElements = _countof(layout);
	gpsDesc.InputLayout.pInputElementDescs = layout;
	gpsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpsDesc.SampleDesc.Count = 1;
	gpsDesc.SampleDesc.Quality = 0;
	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	gpsDesc.pRootSignature = m_peraRS.Get();

	result = m_dev->CreateGraphicsPipelineState(
		&gpsDesc, IID_PPV_ARGS(m_peraPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

    return S_OK;
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

	for (unsigned int i = 0; i < swcDesc.BufferCount; ++i) {
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
		// assert(0);
		return nullptr;
	}

	auto img = scrachImg.GetImage(0, 0, 0);	// 生の画像データ取得

	// WriteToSubresourceで転送するためのヒープ設定
	// GPU上のリソースにテクスチャをコピーする
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
											   D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(texMeta.format,
												texMeta.width, static_cast<UINT>(texMeta.height),
												static_cast<UINT16>(texMeta.arraySize),
									            static_cast<UINT16>(texMeta.mipLevels));

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
	result = texBuff->WriteToSubresource(0, nullptr,
					img->pixels, (UINT)img->rowPitch, (UINT)img->slicePitch);
	if (FAILED(result)) {
		assert(0);
		return nullptr;
	}
	return texBuff;
}

bool DX12Wrapper::createTextureBuffer(ComPtr<ID3D12Resource>& texBuff, size_t width, size_t height)
{
    auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, (UINT)height);
    auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
        D3D12_MEMORY_POOL_L0);

    auto result = m_dev->CreateCommittedResource(
        &texHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(texBuff.ReleaseAndGetAddressOf())
    );
    if (FAILED(result)) {
        assert(SUCCEEDED(result));
		return false;
    }
	return true;
}

bool DX12Wrapper::createWhiteTexture()
{
	if (!createTextureBuffer(m_whiteTex, 4, 4)) {
		return false;
	}

    std::vector<unsigned char> data(4 * 4 * 4);
    std::fill(data.begin(), data.end(), 0xff);

    auto result = m_whiteTex->WriteToSubresource(0, nullptr,
				data.data(), 4 * 4, static_cast<UINT>(data.size()));

	if (FAILED(result)) {
		assert(0);
		return false;
	}
    return true;
}

bool DX12Wrapper::createBlackTexture()
{
	if (!createTextureBuffer(m_blackTex, 4, 4)) {
		return false;
	}
    std::vector<unsigned char> data(4 * 4 * 4);
    std::fill(data.begin(), data.end(), 0x00);

    auto result = m_blackTex->WriteToSubresource(0, nullptr,
				data.data(), 4 * 4, static_cast<UINT>(data.size()));
	if (FAILED(result)) {
		assert(0);
		return false;
	}
    return true;
}

bool DX12Wrapper::createGrayGradationTexture()
{
	if (!createTextureBuffer(m_gradTex, 4, 256)) {
		return false;
	}

    std::vector<unsigned int> data(4 * 256);
    auto it = data.begin();
    unsigned int c = 0xff;
    for (; it != data.end(); it += 4) {
        auto col = (c << 24) | (c << 16) | (c << 8) | c;
        std::fill(it, it + 4, col);
        --c;
    }
    auto result = m_gradTex->WriteToSubresource(0, nullptr,
			data.data(), 4 * 4, static_cast<UINT>(data.size()));
	if (FAILED(result)) {
		assert(0);
		return false;
	}
	return true;
}
