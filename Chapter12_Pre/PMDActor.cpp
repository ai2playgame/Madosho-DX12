#include "PMDActor.hpp"
#include "PMDRenderer.hpp"
#include "Dx12Wrapper.hpp"
#include "PathOperator.hpp"
#include <d3dx12.h>

#pragma comment(lib, "winmm.lib")

using namespace Microsoft::WRL;
using namespace DirectX;

// ---------------------------------------------------------------- //
//  無名名前空間
// ---------------------------------------------------------------- //
namespace {
	// PathOperator.hppに移譲
}

// ---------------------------------------------------------------- //
//  内部クラス実装
// ---------------------------------------------------------------- //

// バイト境界を16Byteに固定して動的確保する
void* PMDActor::Transform::operator new(size_t size) {
	return _aligned_malloc(size, 16);
}

// KeyFrame構造体のコンストラクタ定義
PMDActor::KeyFrame::KeyFrame(unsigned int fno, const DirectX::XMVECTOR& q, const DirectX::XMFLOAT2& ip1, const DirectX::XMFLOAT2& ip2)
	: frameNo{ fno }
	, quaternion{ q }
	, p1{ ip1 }
	, p2{ ip2 }
{}

// ---------------------------------------------------------------- //
//  publicメソッド実装
// ---------------------------------------------------------------- //

PMDActor::PMDActor(const char* filepath, DX12Wrapper& dxRef)
	: m_dx12Ref(dxRef)
	, m_pos(0, 0, 0)
	, m_rotator(0, 0, 0)
{
	// m_transformを更新
	updateTransform();

	loadPMDFile(filepath);
	createTransformView();
	createMaterialData();
	createMaterialAndTextureView();
}

void PMDActor::draw() {
	m_dx12Ref.commandList()->IASetVertexBuffers(0, 1, &m_vbView);
	m_dx12Ref.commandList()->IASetIndexBuffer(&m_ibView);

	ID3D12DescriptorHeap* transheaps[] = {m_transformHeap.Get()};
	m_dx12Ref.commandList()->SetDescriptorHeaps(1, transheaps);
	m_dx12Ref.commandList()->SetGraphicsRootDescriptorTable(1, m_transformHeap->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* mdh[] = { m_materialHeap.Get() };
	// マテリアル
	m_dx12Ref.commandList()->SetDescriptorHeaps(1, mdh);

	auto materialH = m_materialHeap->GetGPUDescriptorHandleForHeapStart();
	unsigned int idxOffset = 0;

	// マテリアルとテクスチャとSPHの合計5
	auto cbvsrvIncSize = m_dx12Ref.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
	for (auto& m : m_materials) {
		m_dx12Ref.commandList()->SetGraphicsRootDescriptorTable(2, materialH);
		m_dx12Ref.commandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}

}

void PMDActor::playAnimation()
{
	m_startTime = timeGetTime();
}

void PMDActor::move(float x, float y, float z)
{
	m_pos.x += x;
	m_pos.y += y;
	m_pos.z += z;
	updateTransform();
}

void PMDActor::rotate(float x, float y, float z)
{
	m_rotator.x += x;
	m_rotator.y += y;
	m_rotator.z += z;
	updateTransform();
}

void PMDActor::update() {
	// m_angle += 0.01f;
	updateTransform();
    m_mappedMatrices[0] = m_transform.world;
	motionUpdate();
}

// ---------------------------------------------------------------- //
//  privateメソッド実装
// ---------------------------------------------------------------- //

HRESULT PMDActor::createTransformView() {
	// GPUバッファ作成
	auto buffSize = sizeof(XMMATRIX) * (1 + m_boneMatrices.size());
	buffSize = (buffSize + 0xff)&~0xff;
	auto result = m_dx12Ref.device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_transformBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	// CPU上のデータをGPUバッファにマッピング
	result = m_transformBuff->Map(0, nullptr, (void**)&m_mappedMatrices);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	// [0]にはワールド変換行列
	// [1:]にはボーン行列
	m_mappedMatrices[0] = m_transform.world;
	std::copy(m_boneMatrices.begin(), m_boneMatrices.end(), m_mappedMatrices + 1);

	// ビューの作成
	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc{};
	transformDescHeapDesc.NumDescriptors = 1; // とりあえずワールドひとつ
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;

	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // デスクリプタヒープ種別
	result = m_dx12Ref.device()->CreateDescriptorHeap(&transformDescHeapDesc,
		IID_PPV_ARGS(m_transformHeap.ReleaseAndGetAddressOf())); // 生成
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;
	m_dx12Ref.device()->CreateConstantBufferView(&cbvDesc, m_transformHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

HRESULT PMDActor::createMaterialData() {
	// マテリアルバッファを作成
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff)&~0xff;
	auto result = m_dx12Ref.device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize*m_materials.size()),// 勿体ないけど仕方ないですね
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_materialBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	// GPU上のメモリにマッピング
	char* mapMaterial = nullptr;
	result = m_materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	for (auto& m : m_materials) {
		*((MaterialForHlsl*)mapMaterial) = m.material; // データコピー
		mapMaterial += materialBuffSize; // 次のアライメント位置まで進める
	}
	m_materialBuff->Unmap(0, nullptr);

	return S_OK;

}

HRESULT PMDActor::createMaterialAndTextureView() {
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.NumDescriptors = m_materials.size() * 5; // マテリアル数ぶん(定数1つ、テクスチャ3つ)
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // デスクリプタヒープ種別
	auto result = m_dx12Ref.device()->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(m_materialHeap.ReleaseAndGetAddressOf()));//生成
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	
	// マテリアルの定数データ（ディフューズetc……）
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff)&~0xff;
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc{};
	matCBVDesc.BufferLocation = m_materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize;
	
	// マテリアルに対応したテクスチャ用のビュー作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // TODO:後述
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1; // ミップマップは使用しないので1

	// マテリアル数分CBV+(SRV+SRV+SRV+SRV)、CBV+(SRV+SRV+SRV+SRV)……とビューをディスクリプタヒープに並べる
	// incSize: CBV,SRV,UAVのディスクリプタサイズ
	auto incSize = m_dx12Ref.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH(m_materialHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < m_materials.size(); ++i) {
		// マテリアル用CBV作成
		m_dx12Ref.device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		matCBVDesc.BufferLocation += materialBuffSize;
		matDescHeapH.ptr += incSize;

		// BaseColorテクスチャ用SRV作成
		if (m_textureResources[i] == nullptr) {
			srvDesc.Format = m_dx12Ref.whiteTexture()->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_dx12Ref.whiteTexture().Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = m_textureResources[i]->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_textureResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;
		
		// SPHテクスチャ用のSRV作成
		if (m_sphResources[i] == nullptr) {
			srvDesc.Format = m_dx12Ref.whiteTexture()->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_dx12Ref.whiteTexture().Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = m_sphResources[i]->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_sphResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		// SPAテクスチャ用のSRV作成
		if (m_spaResources[i] == nullptr) {
			srvDesc.Format = m_dx12Ref.blackTexture()->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_dx12Ref.blackTexture().Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = m_spaResources[i]->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_spaResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		// Toonテクスチャ用のSRV作成
		if (m_toonResources[i] == nullptr) {
			srvDesc.Format = m_dx12Ref.gradTexture()->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_dx12Ref.gradTexture().Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = m_toonResources[i]->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_toonResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;
	}
}

HRESULT PMDActor::loadPMDFile(const char* path) {
	// PMDヘッダ構造体
	struct PMDHeader {
		float version; // 例：00 00 80 3F == 1.00
		char model_name[20]; // モデル名
		char comment[256]; // モデルコメント
	};
	char signature[3];
	PMDHeader pmdheader{};

	std::string strModelPath = path;

	auto fp = fopen(strModelPath.c_str(), "rb");
	if (fp == nullptr) {
		assert(0);
		return ERROR_FILE_NOT_FOUND;
	}
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	unsigned int vertNum; // 頂点数
	fread(&vertNum, sizeof(vertNum), 1, fp);


	// ---------------------------------------------------------------- //
    //  頂点情報読み込み（GPUバッファに書き込み・ビュー作成） 
    // ---------------------------------------------------------------- //
	constexpr unsigned int pmdvertex_size = 38; // 頂点1つあたりのサイズ
	std::vector<unsigned char> vertices(vertNum * pmdvertex_size); // バッファ確保
	fread(vertices.data(), vertices.size(), 1, fp); // 一気に読み込み

	unsigned int indicesNum; // インデックス数
	fread(&indicesNum, sizeof(indicesNum), 1, fp);

	auto result = m_dx12Ref.device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertices.size()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_vb.ReleaseAndGetAddressOf()));

	unsigned char* vertMap = nullptr;
	result = m_vb->Map(0, nullptr, (void**)&vertMap);
	std::copy(vertices.begin(), vertices.end(), vertMap);
	m_vb->Unmap(0, nullptr);

	m_vbView.BufferLocation = m_vb->GetGPUVirtualAddress(); // バッファの仮想アドレス
	m_vbView.SizeInBytes = vertices.size(); // 全バイト数
	m_vbView.StrideInBytes = pmdvertex_size; // 1頂点あたりのバイト数

	// ---------------------------------------------------------------- //
    //  インデックス情報読み込み（GPUバッファに書き込み・ビュー作成） 
    // ---------------------------------------------------------------- //
	std::vector<unsigned short> indices(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp); // 一気に読み込み

	// 設定は、バッファのサイズ以外頂点バッファの設定を使いまわしてOK?
	result = m_dx12Ref.device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0])),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_ib.ReleaseAndGetAddressOf()));

	// 作ったバッファにインデックスデータをコピー
	unsigned short* mappedIdx = nullptr;
	m_ib->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(indices.begin(), indices.end(), mappedIdx);
	m_ib->Unmap(0, nullptr);

	// インデックスバッファビューを作成
	m_ibView.BufferLocation = m_ib->GetGPUVirtualAddress();
	m_ibView.Format = DXGI_FORMAT_R16_UINT;
	m_ibView.SizeInBytes = indices.size() * sizeof(indices[0]);

	// ---------------------------------------------------------------- //
    //  マテリアル読み込み 
    // ---------------------------------------------------------------- //
#pragma pack(1) // ここから1バイトパッキング…アライメントは発生しない
	// 生のPMDマテリアル構造体
	struct PMDMaterial {
		XMFLOAT3 diffuse; // ディフューズ色
		float alpha; // ディフューズα
		float specularity;// スペキュラの強さ(乗算値)
		XMFLOAT3 specular; // スペキュラ色
		XMFLOAT3 ambient; // アンビエント色
		unsigned char toonIdx; // トゥーン番号(後述)
		unsigned char edgeFlg;// マテリアル毎の輪郭線フラグ
		// 2バイトのパディングが発生！！
		unsigned int indicesNum; // このマテリアルが割り当たるインデックス数
		char texFilePath[20]; // テクスチャファイル名(プラスアルファ…後述)
	}; // 70バイトのはず…でもパディングが発生するため72バイト
#pragma pack() // 1バイトパッキング解除

	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);
	m_materials.resize(materialNum);
	m_textureResources.resize(materialNum);
	m_sphResources.resize(materialNum);
	m_spaResources.resize(materialNum);
	m_toonResources.resize(materialNum);

	std::vector<PMDMaterial> pmdMaterials(materialNum);
	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);
	// メンバの動的配列に読み込んだマテリアル情報を書き込み
	for (int i = 0; i < pmdMaterials.size(); ++i) {
		m_materials[i].indicesNum = pmdMaterials[i].indicesNum;
		m_materials[i].material.diffuse = pmdMaterials[i].diffuse;
		m_materials[i].material.alpha = pmdMaterials[i].alpha;
		m_materials[i].material.specular = pmdMaterials[i].specular;
		m_materials[i].material.specularity = pmdMaterials[i].specularity;
		m_materials[i].material.ambient = pmdMaterials[i].ambient;
		m_materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;
	}

	for (int i = 0; i < pmdMaterials.size(); ++i) {
		// トゥーンリソースの読み込み
		char toonFilePath[32];
		sprintf(toonFilePath, "toon/toon%02d.bmp", (unsigned char)(pmdMaterials[i].toonIdx + 1));
		m_toonResources[i] = m_dx12Ref.getTextureByPath(toonFilePath);

		if (strlen(pmdMaterials[i].texFilePath) == 0) {
			m_textureResources[i] = nullptr;
			continue;
		}

		std::string texFileName = pmdMaterials[i].texFilePath;
		std::string sphFileName = "";
		std::string spaFileName = "";
		if (std::count(texFileName.begin(), texFileName.end(), '*') > 0) {//スプリッタがある
			auto [first, second] = PathOperator::splitFileName(texFileName);
			if (PathOperator::getExtension(first) == "sph") {
				texFileName = second;
				sphFileName = first;
			}
			else if (PathOperator::getExtension(first) == "spa") {
				texFileName = second;
				spaFileName = first;
			}
			else {
				texFileName = first;
				if (PathOperator::getExtension(second) == "sph") {
					sphFileName = second;
				}
				else if (PathOperator::getExtension(second) == "spa") {
					spaFileName = second;
				}
			}
		}
		else {
			if (PathOperator::getExtension(pmdMaterials[i].texFilePath) == "sph") {
				sphFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else if (PathOperator::getExtension(pmdMaterials[i].texFilePath) == "spa") {
				spaFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else {
				texFileName = pmdMaterials[i].texFilePath;
			}
		}
		// モデルとテクスチャパスからアプリケーションからのテクスチャパスを得る
		if (texFileName != "") {
			auto texFilePath = PathOperator::getTexturePathFromModelAndTexPath(strModelPath, texFileName.c_str());
			m_textureResources[i] = m_dx12Ref.getTextureByPath(texFilePath.c_str());
		}
		if (sphFileName != "") {
			auto sphFilePath = PathOperator::getTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
			m_sphResources[i] = m_dx12Ref.getTextureByPath(sphFilePath.c_str());
		}
		if (spaFileName != "") {
			auto spaFilePath = PathOperator::getTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
			m_spaResources[i] = m_dx12Ref.getTextureByPath(spaFilePath.c_str());
		}
	}

	// ---------------------------------------------------------------- //
    //  ボーン読み込み
    // ---------------------------------------------------------------- //
	unsigned short boneNum = 0;
	fread(&boneNum, sizeof(boneNum), 1, fp);
#pragma pack(1)
	struct Bone { // ボーン読み込み用一時構造体
		char boneName[20]; // ボーン名
		unsigned short parentNo; // 親ボーン番号
		unsigned short nextNo; // 先端のボーン番号
		unsigned char type; // ボーン種別
		unsigned short ikBoneNo; // IKボーン番号
		XMFLOAT3 pos; // ボーンの基準点座標
	};
#pragma pack()

	// ファイルからボーン情報を実際に読み込む
	std::vector<Bone> pmdBones(boneNum);
	fread(pmdBones.data(), sizeof(Bone), boneNum, fp);

	// インデックスと名前の対応付のために必要 
	std::vector<std::string> boneNames(pmdBones.size());
	// ボーンの木構造を作成する
	for (int idx = 0; idx < pmdBones.size(); ++idx) {
		auto& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;
		auto& node = m_boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
	}
	// 親子関係を構築
	for (auto& pb : pmdBones) {
		// 親のインデックス番号をチェック（無効値なら飛ばす）
		if (pb.parentNo >= pmdBones.size()) {
			continue;
		}
		auto parentName = boneNames[pb.parentNo];
		m_boneNodeTable[parentName].children.emplace_back(&m_boneNodeTable[pb.boneName]);
	}
	m_boneMatrices.resize(pmdBones.size());

	// ボーンを全て初期化する
	std::fill(m_boneMatrices.begin(), m_boneMatrices.end(), XMMatrixIdentity());

	fclose(fp);
}

HRESULT PMDActor::loadVMDFile(const char* path, const char* name)
{
	auto fp = fopen(path, "rb");
	// 最初の50バイトは読み飛ばす
	fseek(fp, 50, SEEK_SET);
	unsigned int keyframeNum = 0;
	fread(&keyframeNum, sizeof(keyframeNum), 1, fp);

	struct VMDKeyFrame {
		char boneName[15];
		unsigned int frameNo; // フレーム番号
		XMFLOAT3 location;
		XMFLOAT4 quaternion;
		unsigned char bezier[64];
	};
	std::vector<VMDKeyFrame> keyframes(keyframeNum);
	for (auto& keyframe : keyframes) {
		fread(keyframe.boneName, sizeof(keyframe.boneName), 1, fp);
		fread(&keyframe.frameNo, sizeof(keyframe.frameNo)
				+ sizeof(keyframe.location)
				+ sizeof(keyframe.quaternion)
                + sizeof(keyframe.bezier), 1, fp);
		
		// 最大フレーム番号を記録
		m_lastFrameNum = std::max<unsigned int>(m_lastFrameNum, keyframe.frameNo);
	}

	// VMDから読みだしたデータを辞書型に変換して使いやすく
	for (auto& f : keyframes) {
		m_motionData[f.boneName].emplace_back(
			f.frameNo,
			XMLoadFloat4(&f.quaternion),
			XMFLOAT2(static_cast<float>(f.bezier[3] / 127.0f), static_cast<float>(f.bezier[7] / 127.0f)),
		    XMFLOAT2(static_cast<float>(f.bezier[11] / 127.0f), static_cast<float>(f.bezier[15] / 127.0f)));
	}

	for (auto& motion : m_motionData) {
		std::sort(motion.second.begin(), motion.second.end(), 
			[](const KeyFrame& lhs, const KeyFrame& rhs) { return lhs.frameNo <= rhs.frameNo;} );
	}

	// クォータニオンを使って実際に回転させる
	for (auto& bonemotion : m_motionData) {
		auto node = m_boneNodeTable[bonemotion.first];
		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			* XMMatrixRotationQuaternion(bonemotion.second[0].quaternion)
			* XMMatrixTranslation(pos.x, pos.y, pos.z);
		m_boneMatrices[node.boneIdx] = mat;
	}

	recursiveMatrixMultipy(&m_boneNodeTable["センター"], XMMatrixIdentity());
	std::copy(m_boneMatrices.begin(), m_boneMatrices.end(), m_mappedMatrices + 1);

	return S_OK;
}

float PMDActor::getYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n)
{
    if (a.x == a.y&&b.x == b.y)return x;//計算不要
	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x;//t^3の係数
	const float k1 = 3 * b.x - 6 * a.x;//t^2の係数
	const float k2 = 3 * a.x;//tの係数

	//誤差の範囲内かどうかに使用する定数
	constexpr float epsilon = 0.0005f;

	for (int i = 0; i < n; ++i) {
		//f(t)求めまーす
		auto ft = k0 * t*t*t + k1 * t*t + k2 * t - x;
		//もし結果が0に近い(誤差の範囲内)なら打ち切り
		if (ft <= epsilon && ft >= -epsilon)break;

		t -= ft / 2;
	}
	//既に求めたいtは求めているのでyを計算する
	auto r = 1 - t;
	return t * t*t + 3 * t*t*r*b.y + 3 * t*r*r*a.y;
	return 0.0f;
}

void PMDActor::recursiveMatrixMultipy(BoneNode* node, const DirectX::XMMATRIX& mat)
{
	m_boneMatrices[node->boneIdx] *= mat;
	for (auto& cnode : node->children) {
		recursiveMatrixMultipy(cnode, m_boneMatrices[node->boneIdx]);
	}
}

void PMDActor::motionUpdate(){
	auto elapsedTime = timeGetTime() - m_startTime;//経過時間を測る
	unsigned int frameNo = 30 * (elapsedTime / 1000.0f);

	//行列情報クリア(してないと前フレームのポーズが重ね掛けされてモデルが壊れる)
	std::fill(m_boneMatrices.begin(), m_boneMatrices.end(), XMMatrixIdentity());

	//モーションデータ更新
	for (auto& bonemotion : m_motionData) {
		auto node = m_boneNodeTable[bonemotion.first];
		//合致するものを探す
		auto keyframes = bonemotion.second;

		auto rit=find_if(keyframes.rbegin(), keyframes.rend(), [frameNo](const KeyFrame& keyframe) {
			return keyframe.frameNo <= frameNo;
		});
		if (rit == keyframes.rend())continue;//合致するものがなければ飛ばす
		XMMATRIX rotation;
		auto it = rit.base();
		if (it != keyframes.end()) {
		auto t = static_cast<float>(frameNo - rit->frameNo) / 
				static_cast<float>(it->frameNo - rit->frameNo);
		t = getYFromXOnBezier(t, it->p1, it->p2, 12);

			rotation = XMMatrixRotationQuaternion(
						XMQuaternionSlerp(rit->quaternion,it->quaternion,t)
					);
		}
		else {
			rotation=XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z) * //原点に戻し
			rotation * // 回転し
			XMMatrixTranslation(pos.x, pos.y, pos.z); // 元の座標に戻す
		m_boneMatrices[node.boneIdx] = mat;
	}
	recursiveMatrixMultipy(&m_boneNodeTable["センター"], XMMatrixIdentity());
	std::copy(m_boneMatrices.begin(), m_boneMatrices.end(), m_mappedMatrices + 1);

	// アニメーションループ
	if (frameNo > m_lastFrameNum) {
		m_startTime = timeGetTime();
	}
}

void PMDActor::updateTransform()
{
	m_transform.world = XMMatrixRotationRollPitchYaw(m_rotator.x, m_rotator.y, m_rotator.z)
		* XMMatrixTranslation(m_pos.x, m_pos.y, m_pos.z);
}

