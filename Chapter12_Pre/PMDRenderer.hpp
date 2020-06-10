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

// ---------------------------------------------------------------- //
//	privateメソッド
// ---------------------------------------------------------------- //

    // パイプラインの初期化
    HRESULT createGraphicsPipelineForPMD();
    // ルートシグネチャの初期化
    HRESULT createRootSignature();

    bool checkShaderCompileResult(HRESULT result, ID3DBlob* err = nullptr);

};
