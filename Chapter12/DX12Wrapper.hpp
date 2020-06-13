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

// ---------------------------------------------------------------- //
//  前方宣言
// ---------------------------------------------------------------- //
class PMDRenderer;

class DX12Wrapper
{
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

public:

	DX12Wrapper(HWND hwnd);
	~DX12Wrapper() = default;

	void update();
	// void setScene();
	void clear();
	void flip();

	// TODO:
	void draw(std::shared_ptr<PMDRenderer> renderer);

	// テクスチャパスから必要なテクスチャバッファへのポインタを返す
	// テクスチャファイルパス
	ComPtr<ID3D12Resource> getTextureByPath(const char* texpath);

	// ペラポリ1つ目をDrawする前に呼び出す
	HRESULT preDrawToPera1();
	// ペラポリ1つ目をDrawした後に呼び出す
	void postDrawToPera1();
	// ペラポリ1つ目の描画
	void drawToPera1(std::shared_ptr<PMDRenderer> renderer);

	// ---------------------------------------------------------------- //
    //  public Getter
    // ---------------------------------------------------------------- //

	ComPtr<ID3D12Device> device();// デバイス
	ComPtr<ID3D12GraphicsCommandList> commandList();// コマンドリスト
	ComPtr<IDXGISwapChain4> swapchain();// スワップチェイン

	ComPtr<ID3D12Resource> whiteTexture();
	ComPtr<ID3D12Resource> blackTexture();
	ComPtr<ID3D12Resource> gradTexture();

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

	// 白・黒・グラデーションテクスチャ
	ComPtr<ID3D12Resource> m_whiteTex;
	ComPtr<ID3D12Resource> m_blackTex;
	ComPtr<ID3D12Resource> m_gradTex;

	// マルチパスレンダリングの為の平面ポリゴン用メンバ
	ComPtr<ID3D12Resource> m_peraResource;
	ComPtr<ID3D12DescriptorHeap> m_peraRTVHeap; // RTVとして扱う時のビュー
	ComPtr<ID3D12DescriptorHeap> m_peraSRVHeap; // SRVとして扱う時のビュー
	ComPtr<ID3D12Resource> m_peraVB;	// 平面ポリゴン頂点バッファ
	D3D12_VERTEX_BUFFER_VIEW m_peraVBView; // 頂点バッファ用ビュー
	ComPtr<ID3D12RootSignature> m_peraRS; // ルートシグネチャ
	ComPtr<ID3D12PipelineState> m_peraPipeline; // パイプラインステート

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

	// ペラポリゴンのリソースとビューの生成
	HRESULT createPeraResourceAndView();

	// ペラポリ頂点作成
	HRESULT createPeraVertex();

	// ペラポリ描画用のパイプラインステート作成
	HRESULT createPeraPipeline();

	// テクスチャローダテーブルの作成
	void createTextureLoaderTable();

	// テクスチャ名からテクスチャバッファ作成、中身をコピー
	ID3D12Resource* createTextureFromFile(const char* texpath);

    bool createTextureBuffer(ComPtr<ID3D12Resource>& texBuff, size_t width, size_t height);

    // 白単色テクスチャの作成
    bool createWhiteTexture();
    // 黒単色テクスチャの作成
    bool createBlackTexture();
    // グレースケールグラデーションの作成
    bool createGrayGradationTexture();

};
