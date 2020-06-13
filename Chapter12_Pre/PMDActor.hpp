#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>
#include <Windows.h>
#include <unordered_map>

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

    // コンストラクタ・デストラクタ宣言
    PMDActor(const char *filepath, DX12Wrapper& dxRef);
    ~PMDActor() = default;  // デストラクタはデフォルト実装

    void update();

    void draw();

    void playAnimation();

    void move(float x, float y, float z);
    void rotate(float x, float y, float z);

    // VMDファイル（アニメーション）のロード
    HRESULT loadVMDFile(const char* path);

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

    struct BoneNode {
        int boneIdx;
        DirectX::XMFLOAT3 startPos;
        std::vector<BoneNode*> children;
    };

    struct KeyFrame {
        unsigned int frameNo; // フレーム番号
        DirectX::XMVECTOR quaternion; // クォータニオン
        DirectX::XMFLOAT2 p1, p2; // ベジェの中間コントロールポイント
        KeyFrame(unsigned int fno, const DirectX::XMVECTOR& q, const DirectX::XMFLOAT2& ip1, const DirectX::XMFLOAT2& ip2);
    };

    // ---------------------------------------------------------------- //
    //	private メンバ
    // ---------------------------------------------------------------- //

    // 依存モジュール
    // TODO: 参照型をメンバに持つのは危険では？
    DX12Wrapper& m_dx12Ref;

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

    // [0]にはワールド変換行列を、[1:]にはボーン行列群を入れる
    DirectX::XMMATRIX* m_mappedMatrices = nullptr;
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

    // ボーン関連
    std::vector<DirectX::XMMATRIX> m_boneMatrices;
    std::unordered_map<std::string, BoneNode> m_boneNodeTable;

    // アニメーション開始時点のミリ秒時間
    DWORD m_startTime;
    
    unsigned int m_lastFrameNum = 0;

    // モーションデータ保持
    std::unordered_map<std::string, std::vector<KeyFrame>> m_motionData;

    DirectX::XMFLOAT3 m_rotator;
    DirectX::XMFLOAT3 m_pos;

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

    float getYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n = 12);

    void recursiveMatrixMultipy(BoneNode* node, const DirectX::XMMATRIX& mat);

    void motionUpdate();

    // TODO: 冗長。最後にトランスフォーム行列計算すれば十分
    void updateTransform();
};
