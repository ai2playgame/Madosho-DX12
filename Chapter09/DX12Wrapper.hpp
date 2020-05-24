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

	// テクスチャパスから必要なテクスチャバッファへのポインタを返す
	// テクスチャファイルパス
	ComPtr<ID3D12Resource> getTextureByPath(const char* texpath);

	ComPtr<ID3D12Device> device();// デバイス
	ComPtr<ID3D12GraphicsCommandList> commandList();// コマンドリスト
	ComPtr<IDXGISwapChain4> swapchain();// スワップチェイン


private:

	// ---------------------------------------------------------------- //
	//	privateメンバ変数
	// ---------------------------------------------------------------- //

	// 使用するGPUのデバイス名
	const std::wstring DEV_NAME = L"NVIDIA";

	// ウィンドウサイズ
	SIZE m_winSize;

	// DXGIまわり
	ComPtr<IDXGIFactory4> m_dxgiFactory = nullptr;// DXGIインターフェイス
	ComPtr<IDXGISwapChain4> m_swapchain = nullptr;// スワップチェイン

	// DirectX12まわり
	ComPtr<ID3D12Device> m_dev = nullptr;// デバイス
	ComPtr<ID3D12CommandAllocator> m_cmdAllocator = nullptr;// コマンドアロケータ
	ComPtr<ID3D12GraphicsCommandList> m_cmdList = nullptr;// コマンドリスト
	ComPtr<ID3D12CommandQueue> m_cmdQueue = nullptr;// コマンドキュー

	// 表示に関わるバッファ周り
	ComPtr<ID3D12Resource> m_depthBuffer = nullptr;// 深度バッファ
	std::vector<ID3D12Resource*> m_backBuffers;// バックバッファ(2つ以上…スワップチェインが確保)
	ComPtr<ID3D12DescriptorHeap> m_rtvHeaps = nullptr;// レンダーターゲット用デスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;// 深度バッファビュー用デスクリプタヒープ

	std::unique_ptr<D3D12_VIEWPORT> m_viewport; // ビューポート
	std::unique_ptr<D3D12_RECT> m_scissorrect; // シザー矩形

	// シーンを構成するバッファ等
	ComPtr<ID3D12Resource> m_sceneConstBuff = nullptr;

	struct SceneData {
		DirectX::XMMATRIX view;// ビュー行列
		DirectX::XMMATRIX proj;// プロジェクション行列
		DirectX::XMFLOAT3 eye;// 視点座標
	};

	SceneData* m_mappedSceneData;
	ComPtr<ID3D12DescriptorHeap> m_sceneDescHeap = nullptr;

	// フェンス
	ComPtr<ID3D12Fence> m_fence = nullptr;
	UINT64 m_fenceVal = 0;

	// ロード用テーブル
	using LoadLambda_t = std::function<HRESULT(const std::wstring&,
		DirectX::TexMetadata*,
		DirectX::ScratchImage&)>;
	std::map < std::string, LoadLambda_t> m_loadLambdaTable;

	// テクスチャテーブル
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> m_textureTable;

	// ---------------------------------------------------------------- //
	//	privateメソッド宣言				                            
	// ---------------------------------------------------------------- //

	// 最終的なレンダーターゲットの生成
	HRESULT	createFinalRenderTargets();

	// デプスステンシルビューの生成
	HRESULT createDepthStencilView();

	// スワップチェインの生成
	HRESULT createSwapChain(const HWND& hwnd);

	// DXGIまわり初期化
	HRESULT initDXGIDevice();

	// コマンドまわり初期化
	HRESULT initCommand();

	// ビュープロジェクション用ビューの生成
	HRESULT createSceneView();

	// テクスチャローダテーブルの作成
	void createTextureLoaderTable();

	// テクスチャ名からテクスチャバッファ作成、中身をコピー
	ID3D12Resource* createTextureFromFile(const char* texpath);

};