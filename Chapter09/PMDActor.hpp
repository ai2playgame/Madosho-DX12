#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

// ---------------------------------------------------------------- //
//	前方宣言
// ---------------------------------------------------------------- //
class DX12Wrapper;
class PMDRenderer;

class PMDActor {
    // ---------------------------------------------------------------- //
    //	friendクラス設定
    // ---------------------------------------------------------------- //
    friend PMDRenderer;

    // ---------------------------------------------------------------- //
    // usingエイリアス
    // ---------------------------------------------------------------- //
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    // ---------------------------------------------------------------- //
    //	public メソッド
    // ---------------------------------------------------------------- //
    PMDActor(const char *filepath, PMDRenderer &renderer, float angle = 0.f);
    ~PMDActor() = default;
    // クローンする際は頂点およびマテリアルは共通のバッファを見るようにする
    PMDActor *clone();

    void update();

    void draw();

private:
    // ---------------------------------------------------------------- //
    //	内部クラス
    // ---------------------------------------------------------------- //

    // シェーダ側に投げられるマテリアルデータ
    struct MaterialForHlsl {
        DirectX::XMFLOAT3 diffuse;  // ディフューズ色
        float alpha;                //  ディフューズのアルファ
        DirectX::XMFLOAT3 specular; // スペキュラ色
        float specularity;          // スペキュラの強さ(乗算値)
        DirectX::XMFLOAT3 ambient;  // アンビエント色
    };

    // シェーダには投げられないマテリアルデータ
    struct AdditionalMaterial {
        std::string texPath; // テクスチャファイルパス
        int toonIdx;         // トゥーン番号
        bool edgeFlg;        // マテリアル毎の輪郭線フラグ
    };

    // マテリアル
    struct Material {
        unsigned int indicesNum; // インデックス数
        MaterialForHlsl material;
        AdditionalMaterial additional;
    };

    struct Transform {
        // 内部に持ってるXMMATRIXメンバが16バイトアライメントであるため
        // Transformをnewする際には16バイト境界に確保する
        void *operator new(size_t size);
        DirectX::XMMATRIX world;
    };

    // ---------------------------------------------------------------- //
    //	private メンバ
    // ---------------------------------------------------------------- //

    // 依存モジュール
    // TODO: 参照型をメンバに持つのは危険では？
    PMDRenderer &m_rendererRef;
    DX12Wrapper &m_dx12Ref;

    // 頂点関連
    ComPtr<ID3D12Resource> m_vb = nullptr;
    ComPtr<ID3D12Resource> m_ib = nullptr;
    D3D12_VERTEX_BUFFER_VIEW m_vbView{};
    D3D12_INDEX_BUFFER_VIEW m_ibView{};

    // 座標変換行列
    ComPtr<ID3D12Resource> m_transformMat = nullptr;
    // 座標変換行列用ディスクリプタヒープ
    ComPtr<ID3D12DescriptorHeap> m_transformHeap = nullptr;

    Transform m_transform;
    Transform *m_mappedTransform = nullptr;
    ComPtr<ID3D12Resource> m_transformBuff = nullptr;

    // マテリアル関連
    std::vector<Material> m_materials;
    ComPtr<ID3D12Resource> m_materialBuff = nullptr;
    std::vector<ComPtr<ID3D12Resource>> m_textureResources;
    std::vector<ComPtr<ID3D12Resource>> m_sphResources;
    std::vector<ComPtr<ID3D12Resource>> m_spaResources;
    std::vector<ComPtr<ID3D12Resource>> m_toonResources;

    // マテリアル用のヒープ（5個分？）
    ComPtr<ID3D12DescriptorHeap> m_materialHeap = nullptr;

    // 動作確認用のY軸回転角
    float m_angle;

    // ---------------------------------------------------------------- //
    //	private メソッド
    // ---------------------------------------------------------------- //

    // 座標変換用ビューの生成
    HRESULT createTransformView();

    // 読み込んだマテリアルをもとにマテリアルバッファを作成
    HRESULT createMaterialData();

    // マテリアル＆テクスチャのビューを作成
    HRESULT createMaterialAndTextureView();

    // PMDファイルのロード
    HRESULT loadPMDFile(const char *path);
};
