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
//  publicメンバ 
// ---------------------------------------------------------------- //
    std::shared_ptr<DX12Wrapper> m_dx12;


// ---------------------------------------------------------------- //
//  publicメソッド
// ---------------------------------------------------------------- //
    PMDRenderer(std::shared_ptr<DX12Wrapper> dx12);

    ~PMDRenderer();
    
    void addActor(std::shared_ptr<PMDActor> actor);

    void update();
    void beginAnimation();
    void draw();

    void beforeDraw();

// ---------------------------------------------------------------- //
//  getter
// ---------------------------------------------------------------- //
    ID3D12PipelineState* pipelineState();
    ID3D12RootSignature* rootSignature();

private:
// ---------------------------------------------------------------- //
//  usingエイリアス
// ---------------------------------------------------------------- //
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    template <typename T>
    using Container = std::vector<T>;

// ---------------------------------------------------------------- //
//  privateメンバ
// ---------------------------------------------------------------- //

    // PMD用のパイプライン
    ComPtr<ID3D12PipelineState> m_pipeline = nullptr;
    // PMD用のルートシグネチャ
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    // Rendererが描画するアクターの参照
    Container<std::shared_ptr<PMDActor>> m_actors;

// ---------------------------------------------------------------- //
//	privateメソッド
// ---------------------------------------------------------------- //

    // パイプラインの初期化
    HRESULT createGraphicsPipelineForPMD();
    // ルートシグネチャの初期化
    HRESULT createRootSignature();

    bool checkShaderCompileResult(HRESULT result, ID3DBlob* err = nullptr);

};
