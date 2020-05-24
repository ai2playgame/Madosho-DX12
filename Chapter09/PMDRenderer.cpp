#include "PMDRenderer.hpp"
#include <d3dx12.h>
#include <cassert>
#include <d3dcompiler.h>
#include "DX12Wrapper.hpp"
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

PMDRenderer::PMDRenderer(DX12Wrapper& dx12)
    : m_dx12Ref(dx12)
{
    assert(SUCCEEDED(createRootSignature()));
    assert(SUCCEEDED(createGraphicsPipelineForPMD()));
    m_whiteTex = createWhiteTexture();
    m_blackTex = createBlackTexture();
    m_gradTex = createGrayGradationTexture();
}

PMDRenderer::~PMDRenderer() {
}

void PMDRenderer::update() {

}

void PMDRenderer::draw() {

}

ID3D12PipelineState* PMDRenderer::getPipelineState() {
    return m_pipeline.Get();
}

ID3D12RootSignature* PMDRenderer::getRootSignature() {
    return m_rootSignature.Get();
}

// ---------------------------------------------------------------- //
//  privateメソッド定義
// ---------------------------------------------------------------- //
ID3D12Resource* PMDRenderer::createDefaultTexture(size_t width, size_t height) {
    auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
        width, height);
    auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
        D3D12_MEMORY_POOL_L0);
    ID3D12Resource* buff = nullptr;
    auto result = m_dx12Ref.device()->CreateCommittedResource(
        &texHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&buff)
    );
    if (FAILED(result)) {
        assert(SUCCEEDED(result));
        return nullptr;
    }
    return buff;
}

ID3D12Resource* PMDRenderer::createWhiteTexture() {
    ID3D12Resource* texBuff = createDefaultTexture(4, 4);
    std::vector<unsigned char> data(4 * 4 * 4);
    std::fill(data.begin(), data.end(), 0xff);

    auto result = texBuff->WriteToSubresource(0, nullptr, data.data(), 4 * 4, data.size());
    assert(SUCCEEDED(result));
    return texBuff;
}

ID3D12Resource* PMDRenderer::createBlackTexture() {
    ID3D12Resource* texBuff = createDefaultTexture(4, 4);
    std::vector<unsigned char> data(4 * 4 * 4);
    std::fill(data.begin(), data.end(), 0x00);

    auto result = texBuff->WriteToSubresource(0, nullptr, data.data(), 4 * 4, data.size());
    assert(SUCCEEDED(result));
    return texBuff;
}

ID3D12Resource* PMDRenderer::createGrayGradationTexture() {
    ID3D12Resource* texBuff = createDefaultTexture(4, 256);
    std::vector<unsigned int> data(4 * 256);
    auto it = data.begin();
    unsigned int c = 0xff;
    for (; it != data.end(); it += 4) {
        auto col = (c << 0xff) | (c << 16) | (c << 8) | c;
        std::fill(it, it + 4, col);
        --c;
    }
    auto result = texBuff->WriteToSubresource(0, nullptr, data.data(), 4 * 4, data.size());
    assert(SUCCEEDED(result));
    return texBuff;
}

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
    result = m_dx12Ref.device()->CreateGraphicsPipelineState(&gpipeline,
        IID_PPV_ARGS(m_pipeline.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        assert(SUCCEEDED(result));
    }
    return result;
}

HRESULT PMDRenderer::createRootSignature() {
    // レンジ設定
    CD3DX12_DESCRIPTOR_RANGE descTblRanges[4] = {}; // テクスチャと定数の2つ
    descTblRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // 定数[b0] （ビュープロジェクション用）
    descTblRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // 定数[b1] （ワールド・ボーン用）
    descTblRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2); // 定数[b2] （マテリアル用）
    descTblRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0); // テクスチャ4つ（基本・sph・spa・トゥーン）

    // ルートパラメタ
    CD3DX12_ROOT_PARAMETER rootParams[3] = {};
    rootParams[0].InitAsDescriptorTable(1, &descTblRanges[0]); // ビュープロジェクション変換
    rootParams[1].InitAsDescriptorTable(1, &descTblRanges[1]); // ワールド・ボーン変換
    rootParams[2].InitAsDescriptorTable(2, &descTblRanges[2]); // マテリアル周り

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
    
    result = m_dx12Ref.device()->CreateRootSignature(
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
