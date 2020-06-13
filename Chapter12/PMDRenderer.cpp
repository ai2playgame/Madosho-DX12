#include "PMDRenderer.hpp"
#include <d3dx12.h>
#include <cassert>
#include <d3dcompiler.h>
#include "DX12Wrapper.hpp"
#include "PMDActor.hpp"
#include <string>
#include <algorithm>

// ---------------------------------------------------------------- //
//  無名名前空間
// ---------------------------------------------------------------- //
namespace {
void printErrBlob(ID3DBlob* blob) {
    assert(blob);
    std::string err;
    err.resize(blob->GetBufferSize());
    std::copy_n((char*)blob->GetBufferPointer(), err.size(), err.begin());
}
}

// ---------------------------------------------------------------- //
//  publicメソッド定義
// ---------------------------------------------------------------- //

PMDRenderer::PMDRenderer(std::shared_ptr<DX12Wrapper> dx12)
    : m_dx12(dx12)
{
    assert(SUCCEEDED(createRootSignature()));
    assert(SUCCEEDED(createGraphicsPipelineForPMD()));
}

PMDRenderer::~PMDRenderer() {
}

void PMDRenderer::addActor(std::shared_ptr<PMDActor> actor)
{
    m_actors.push_back(actor);
}

void PMDRenderer::update() {
    for (auto& actor : m_actors) {
        actor->update();
    }
}

void PMDRenderer::beginAnimation()
{
    for (auto& actor : m_actors) {
        actor->playAnimation();
    }
}

void PMDRenderer::draw() {
    for (auto& actor : m_actors) {
        actor->draw();
    }
}

ID3D12PipelineState* PMDRenderer::pipelineState() {
    return m_pipeline.Get();
}

ID3D12RootSignature* PMDRenderer::rootSignature() {
    return m_rootSignature.Get();
}

// ---------------------------------------------------------------- //
//  privateメソッド定義
// ---------------------------------------------------------------- //

HRESULT PMDRenderer::createGraphicsPipelineForPMD() {
    ComPtr<ID3DBlob> vsBlob = nullptr;
    ComPtr<ID3DBlob> psBlob = nullptr;
    ComPtr<ID3DBlob> errBlob = nullptr;
    
    // 頂点シェーダコンパイル
    // ファイル名 BasicVertexShader.hlsl エントリポイント BasicVS
    auto result = D3DCompileFromFile(L"BasicVertexShader.hlsl",
        nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "BasicVS", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0, &vsBlob, &errBlob);
    if (!checkShaderCompileResult(result, errBlob.Get())) {
        assert(0);
        return result;
    }

    // ピクセルシェーダコンパイル
    // ファイル名 BasicPixelShader.hlsl エントリポイント BasicPS
    result = D3DCompileFromFile(L"BasicPixelShader.hlsl",
        nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "BasicPS", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0, &psBlob, &errBlob);
    if (!checkShaderCompileResult(result, errBlob.Get())) {
        assert(0);
        return result;
    }

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
        { "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
        { "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
        { "BONENO", 0, DXGI_FORMAT_R16G16_UINT,0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "WEIGHT", 0, DXGI_FORMAT_R8_UINT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};
    gpipeline.pRootSignature = m_rootSignature.Get();
    gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
    gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());

    gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 0xfffffff
    gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングしない

    // DSV設定 （ステンシルは使わない）
    gpipeline.DepthStencilState.DepthEnable = true; // 深度バッファ有効
    gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 全て書き込み
    gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // 小さい深度値を採用
    gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    gpipeline.DepthStencilState.StencilEnable = false;
    
    gpipeline.InputLayout.pInputElementDescs = inputLayout; // 入力レイアウト先頭アドレス
    gpipeline.InputLayout.NumElements = _countof(inputLayout); // レイアウト配列数

    gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // ストリップ時のカットなし
    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 三角形で構成
    
    gpipeline.NumRenderTargets = 1; // 今は1つだけ
    gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    gpipeline.SampleDesc.Count = 1; // サンプリングは1ピクセルにつき1
    gpipeline.SampleDesc.Quality = 0; // クオリティは最低

    // パイプライン作成
    result = m_dx12->device()->CreateGraphicsPipelineState(&gpipeline,
        IID_PPV_ARGS(m_pipeline.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        assert(SUCCEEDED(result));
    }
    return result;
}

HRESULT PMDRenderer::createRootSignature() {
    // ディスクリプタレンジ設定
    CD3DX12_DESCRIPTOR_RANGE descTblRanges[4] = {};
    descTblRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // 定数[b0] （ビュープロジェクション用）
    descTblRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // 定数[b1] （ワールド・ボーン用）
    descTblRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2); // 定数[b2] （マテリアルパラメータ）
    descTblRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0); // テクスチャ4つ（BaseColor・sph・spa・トゥーン）

    // ルートパラメータ
    CD3DX12_ROOT_PARAMETER rootParams[3] = {};
    rootParams[0].InitAsDescriptorTable(1, &descTblRanges[0]); // ビュープロジェクション変換
    rootParams[1].InitAsDescriptorTable(1, &descTblRanges[1]); // ワールド・ボーン変換
    rootParams[2].InitAsDescriptorTable(2, &descTblRanges[2]); // マテリアルパラメータ

    CD3DX12_STATIC_SAMPLER_DESC samplerDescs[2] = {};
    samplerDescs[0].Init(0);
    samplerDescs[1].Init(1, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Init(3, rootParams, 2, samplerDescs, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> rootSigBlob = nullptr;
    ComPtr<ID3DBlob> errBlob = nullptr;

    auto result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSigBlob, &errBlob);
    if (FAILED(result)) {
        assert(SUCCEEDED(result));
        return result;
    }
    
    result = m_dx12->device()->CreateRootSignature(
        0, rootSigBlob->GetBufferPointer(),
        rootSigBlob->GetBufferSize(),
        IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())
    );
    if (FAILED(result)) {
        assert(SUCCEEDED(result));
        return result;
    }
    return result;
}

bool PMDRenderer::checkShaderCompileResult(HRESULT result, ID3DBlob* error) {
    if (FAILED(result)) {
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            ::OutputDebugStringA("ファイルが見つかりません");
        }
        else {
            std::string errstr;
            errstr.resize(error->GetBufferSize());
            std::copy_n((char*)error->GetBufferPointer(), error->GetBufferSize(), errstr.begin());
            errstr += "\n";
            ::OutputDebugStringA(errstr.c_str());
        }
        return false;
    }
    else {
        return true;
    }
}
