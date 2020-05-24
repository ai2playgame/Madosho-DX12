#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>

#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void EnableDebugLayer(){
	ID3D12Debug* debugLayer=nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}

/* ===== Global Variables ===== */
constexpr int WINWIDTH = 1280;
constexpr int WINHEIGHT = 720;

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;	
/* ========== */

#ifdef _DEBUG
int main() {
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif

	HRESULT result;
	
	WNDCLASSEX w{};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProc;
	w.lpszClassName = _T("Introduction DX12");
	w.hInstance = GetModuleHandle(nullptr);
	RegisterClassEx(&w);

	RECT wrc = { 0,0, WINWIDTH, WINHEIGHT}; // ウィンドウサイズを決める
	
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // ウィンドウのサイズはちょっと面倒なので関数を使って補正する

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(w.lpszClassName,// クラス名指定
		_T("DX12テスト"),// タイトルバーの文字
		WS_OVERLAPPEDWINDOW,// タイトルバーと境界線があるウィンドウです
		CW_USEDEFAULT,// 表示X座標はOSにお任せします
		CW_USEDEFAULT,// 表示Y座標はOSにお任せします
		wrc.right - wrc.left,// ウィンドウ幅
		wrc.bottom - wrc.top,// ウィンドウ高
		nullptr,// 親ウィンドウハンドル
		nullptr,// メニューハンドル
		w.hInstance,// 呼び出しアプリケーションハンドル
		nullptr);// 追加パラメータ

#ifdef _DEBUG
	// デバッグレイヤーをオンに
	EnableDebugLayer();
#endif 

	// フィーチャレベル列挙
	constexpr D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	// グラフィックボードのアダプターを列挙
#ifdef _DEBUG
	CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif

	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		adapters.push_back(tmpAdapter);
	}
	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc{};
		adpt->GetDesc(&adesc); // アダプターのDesc Object取得
		std::wstring strDesc = adesc.Description;
		// 探したいアダプターの名前を確認（NVIDIA GraphicsBoardを探索）
		// TODO: 危険！
		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}

	// Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(&_dev)) == S_OK) {
			featureLevel = l;
			break;
		}
	}
	// CommandAllocatorとCommandListを作成
	if (FAILED(_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator)))) {
		return -1;
	}
	if (FAILED(_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList)))) {
		return -1;
	}
	// _cmdList->Close();

	// コマンドキュー作成
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc{};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;// タイムアウトなし
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;// プライオリティ特に指定なし
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;// ここはコマンドリストと合わせてください
	if (FAILED(_dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue)))) {
		return -1;
	}

	// スワップチェイン生成
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
	swapchainDesc.Width = WINWIDTH;
	swapchainDesc.Height = WINHEIGHT;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;	// back bafferを可変サイズとする
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	// フレームスワップ後はすぐに破棄
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;	// 特に指定なし(?)
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // ウィンドウモード←→フルスクリーンモード切り替え可能

	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue, // コマンドキュー
		hwnd,	   // ウィンドウハンドル 
		&swapchainDesc, // SwapChain Desc
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapchain);
	if (FAILED(result)) {
		return -1;
	}

	// Descriptor Heapを作成するための設定を用意
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビューなので当然RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;// 表裏の２つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;// 特に指定なし

	// レンダーターゲットビュー(RTV)用のDescriptor Heapを作成
	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	// ディスクリプタとスワップチェーン上のバッファを関連付ける
	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapchain->GetDesc(&swcDesc);
	if (FAILED(result)) { return -1; }

	std::vector<ID3D12Resource*> backBuffers(swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < swcDesc.BufferCount; ++i) {
		// スワップチェーン中のバッファとビューを関連付ける
		if (FAILED(_swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])))){
			return -1;
		}
		// レンダーターゲットビューを生成
		_dev->CreateRenderTargetView(backBuffers[i], nullptr, handle);
		// ポインターをずらす
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	
	// フェンスの生成
	ID3D12Fence* fence = nullptr;
	UINT64 fenceVal = 0;
	result = _dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

	ShowWindow(hwnd, SW_SHOW);

	MSG msg{};
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (msg.message == WM_QUIT) {
			break;
		}

		// バックバッファのインデックスを取得
		UINT bbIdx = _swapchain->GetCurrentBackBufferIndex();
		
		// リソースバリアを設定
		D3D12_RESOURCE_BARRIER BarrierDesc{};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = backBuffers[bbIdx];
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;	// 直前はPRESENT状態
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // この後RT状態になる
		_cmdList->ResourceBarrier(1, &BarrierDesc);		

		// レンダーターゲットを指定
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += static_cast<SIZE_T>(bbIdx) * static_cast<SIZE_T>(_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		_cmdList->OMSetRenderTargets(1, &rtvH , false, nullptr);

		// レンダーターゲットのクリア（黄色）
		float clearColor[] = { 1.f, 1.f, 0.f, 1.f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		// RT状態からPresentに移行する
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		_cmdList->ResourceBarrier(1, &BarrierDesc);

		// 命令のクローズ
		_cmdList->Close();

		// 命令実行
		ID3D12CommandList* cmdLists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdLists);
		
		// 命令が全て完了するのを待つ
		_cmdQueue->Signal(fence, ++fenceVal);

		if (fence->GetCompletedValue() != fenceVal) {
			auto event = CreateEvent(nullptr, false, false, nullptr);
			fence->SetEventOnCompletion(fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
		
		// コマンドリストの中身をクリア
		_cmdAllocator->Reset();
		_cmdList->Reset(_cmdAllocator, nullptr);

		_swapchain->Present(1, 0);
	}
	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;
}