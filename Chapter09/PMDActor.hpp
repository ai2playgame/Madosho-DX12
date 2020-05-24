#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

// ---------------------------------------------------------------- //
//	�O���錾
// ---------------------------------------------------------------- //
class DX12Wrapper;
class PMDRenderer;

class PMDActor {
    // ---------------------------------------------------------------- //
    //	friend�N���X�ݒ�
    // ---------------------------------------------------------------- //
    friend PMDRenderer;

    // ---------------------------------------------------------------- //
    // using�G�C���A�X
    // ---------------------------------------------------------------- //
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    // ---------------------------------------------------------------- //
    //	public ���\�b�h
    // ---------------------------------------------------------------- //
    PMDActor(const char *filepath, PMDRenderer &renderer, float angle = 0.f);
    ~PMDActor() = default;
    // �N���[������ۂ͒��_����у}�e���A���͋��ʂ̃o�b�t�@������悤�ɂ���
    PMDActor *clone();

    void update();

    void draw();

private:
    // ---------------------------------------------------------------- //
    //	�����N���X
    // ---------------------------------------------------------------- //

    // �V�F�[�_���ɓ�������}�e���A���f�[�^
    struct MaterialForHlsl {
        DirectX::XMFLOAT3 diffuse;  // �f�B�t���[�Y�F
        float alpha;                //  �f�B�t���[�Y�̃A���t�@
        DirectX::XMFLOAT3 specular; // �X�y�L�����F
        float specularity;          // �X�y�L�����̋���(��Z�l)
        DirectX::XMFLOAT3 ambient;  // �A���r�G���g�F
    };

    // �V�F�[�_�ɂ͓������Ȃ��}�e���A���f�[�^
    struct AdditionalMaterial {
        std::string texPath; // �e�N�X�`���t�@�C���p�X
        int toonIdx;         // �g�D�[���ԍ�
        bool edgeFlg;        // �}�e���A�����̗֊s���t���O
    };

    // �}�e���A��
    struct Material {
        unsigned int indicesNum; // �C���f�b�N�X��
        MaterialForHlsl material;
        AdditionalMaterial additional;
    };

    struct Transform {
        // �����Ɏ����Ă�XMMATRIX�����o��16�o�C�g�A���C�����g�ł��邽��
        // Transform��new����ۂɂ�16�o�C�g���E�Ɋm�ۂ���
        void *operator new(size_t size);
        DirectX::XMMATRIX world;
    };

    // ---------------------------------------------------------------- //
    //	private �����o
    // ---------------------------------------------------------------- //

    // �ˑ����W���[��
    // TODO: �Q�ƌ^�������o�Ɏ��̂͊댯�ł́H
    PMDRenderer &m_rendererRef;
    DX12Wrapper &m_dx12Ref;

    // ���_�֘A
    ComPtr<ID3D12Resource> m_vb = nullptr;
    ComPtr<ID3D12Resource> m_ib = nullptr;
    D3D12_VERTEX_BUFFER_VIEW m_vbView{};
    D3D12_INDEX_BUFFER_VIEW m_ibView{};

    // ���W�ϊ��s��
    ComPtr<ID3D12Resource> m_transformMat = nullptr;
    // ���W�ϊ��s��p�f�B�X�N���v�^�q�[�v
    ComPtr<ID3D12DescriptorHeap> m_transformHeap = nullptr;

    Transform m_transform;
    Transform *m_mappedTransform = nullptr;
    ComPtr<ID3D12Resource> m_transformBuff = nullptr;

    // �}�e���A���֘A
    std::vector<Material> m_materials;
    ComPtr<ID3D12Resource> m_materialBuff = nullptr;
    std::vector<ComPtr<ID3D12Resource>> m_textureResources;
    std::vector<ComPtr<ID3D12Resource>> m_sphResources;
    std::vector<ComPtr<ID3D12Resource>> m_spaResources;
    std::vector<ComPtr<ID3D12Resource>> m_toonResources;

    // �}�e���A���p�̃q�[�v�i5���H�j
    ComPtr<ID3D12DescriptorHeap> m_materialHeap = nullptr;

    // ����m�F�p��Y����]�p
    float m_angle;

    // ---------------------------------------------------------------- //
    //	private ���\�b�h
    // ---------------------------------------------------------------- //

    // ���W�ϊ��p�r���[�̐���
    HRESULT createTransformView();

    // �ǂݍ��񂾃}�e���A�������ƂɃ}�e���A���o�b�t�@���쐬
    HRESULT createMaterialData();

    // �}�e���A�����e�N�X�`���̃r���[���쐬
    HRESULT createMaterialAndTextureView();

    // PMD�t�@�C���̃��[�h
    HRESULT loadPMDFile(const char *path);
};
