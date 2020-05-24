#pragma once

#include <d3d12.h>
#include <vector>
#include <wrl.h>
#include <memory>

// ---------------------------------------------------------------- //
//  �O���錾
// ---------------------------------------------------------------- //
class DX12Wrapper;
class PMDActor;

class PMDRenderer
{
public:
// ---------------------------------------------------------------- //
//  friend�N���X�ݒ�  
// ---------------------------------------------------------------- //
    friend PMDActor;

// ---------------------------------------------------------------- //
//  public�����o 
// ---------------------------------------------------------------- //
    DX12Wrapper& m_dx12Ref;


// ---------------------------------------------------------------- //
//  public���\�b�h
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
//  using�G�C���A�X
// ---------------------------------------------------------------- //
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

// ---------------------------------------------------------------- //
//  private�����o
// ---------------------------------------------------------------- //

    // PMD�p�̃p�C�v���C��
    ComPtr<ID3D12PipelineState> m_pipeline = nullptr;
    // PMD�p�̃��[�g�V�O�l�`��
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    // PMD�p�̋��ʃe�N�X�`�� (���E���E�O���[�X�P�[���O���f�[�V�����j
    ComPtr<ID3D12Resource> m_whiteTex = nullptr;
    ComPtr<ID3D12Resource> m_blackTex = nullptr;
    ComPtr<ID3D12Resource> m_gradTex = nullptr;

// ---------------------------------------------------------------- //
//	private���\�b�h
// ---------------------------------------------------------------- //

    ID3D12Resource* createDefaultTexture(size_t width, size_t height);

    // ���P�F�e�N�X�`���̍쐬
    ID3D12Resource* createWhiteTexture();
    // ���P�F�e�N�X�`���̍쐬
    ID3D12Resource* createBlackTexture();
    // �O���[�X�P�[���O���f�[�V�����̍쐬
    ID3D12Resource* createGrayGradationTexture();

    // �p�C�v���C���̏�����
    HRESULT createGraphicsPipelineForPMD();
    // ���[�g�V�O�l�`���̏�����
    HRESULT createRootSignature();

    bool checkShaderCompileResult(HRESULT result, ID3DBlob* err = nullptr);

};

