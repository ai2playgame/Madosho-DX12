#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl.h>
#include <memory>

// ---------------------------------------------------------------- //
//  前方宣言
// ---------------------------------------------------------------- //
class DX12Wrapper;
class PMDActor;

class PMDRenderer
{
public:
// ---------------------------------------------------------------- //
//  friendクラス設定  
// ---------------------------------------------------------------- //
    friend PMDActor;

// ---------------------------------------------------------------- //
//  publicメンバ 
// ---------------------------------------------------------------- //
    DX12Wrapper& m_dx12Ref;


// ---------------------------------------------------------------- //
//  publicメソッド
// ---------------------------------------------------------------- //
    PMDRenderer(DX12Wrapper& dx12);

    ~PMDRenderer();

    void update();
    void draw();

// ---------------------------------------------------------------- //
//  getter
// ---------------------------------------------------------------- //
    ID3D12PipelineState* getPipelineState();
    ID3D12RootSignature* getRootSignature();

private:
// ---------------------------------------------------------------- //
//  usingエイリアス
// ---------------------------------------------------------------- //
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

// ---------------------------------------------------------------- //
//  privateメンバ
// ---------------------------------------------------------------- //

    // PMD用のパイプライン
    ComPtr<ID3D12PipelineState> m_pipeline = nullptr;
    // PMD用のルートシグネチャ
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    // PMD用の共通テクスチャ (白・黒・グレースケールグラデーション）
    ComPtr<ID3D12Resource> m_whiteTex = nullptr;
    ComPtr<ID3D12Resource> m_blackTex = nullptr;
    ComPtr<ID3D12Resource> m_gradTex = nullptr;

// ---------------------------------------------------------------- //
//	privateメソッド
// ---------------------------------------------------------------- //

    ID3D12Resource* createDefaultTexture(size_t width, size_t height);

    // 白単色テクスチャの作成
    ID3D12Resource* createWhiteTexture();
    // 黒単色テクスチャの作成
    ID3D12Resource* createBlackTexture();
    // グレースケールグラデーションの作成
    ID3D12Resource* createGrayGradationTexture();

    // パイプラインの初期化
    HRESULT createGraphicsPipelineForPMD();
    // ルートシグネチャの初期化
    HRESULT createRootSignature();

    bool checkShaderCompileResult(HRESULT result, ID3DBlob* err = nullptr);

};
