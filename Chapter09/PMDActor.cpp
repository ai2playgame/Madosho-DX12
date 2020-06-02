#include "PMDActor.hpp"
#include "PMDRenderer.hpp"
#include "Dx12Wrapper.hpp"
#include "PathOperator.hpp"
#include <d3dx12.h>

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

// ---------------------------------------------------------------- //
//  publicメソッド実装
// ---------------------------------------------------------------- //

PMDActor::PMDActor(const char* filepath,PMDRenderer& renderer, float angle)
	: m_rendererRef(renderer)
	, m_dx12Ref(renderer.m_dx12Ref)
	, m_angle(angle)
{
	m_transform.world = XMMatrixIdentity();
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

	auto cbvsrvIncSize = m_dx12Ref.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
	for (auto& m : m_materials) {
		m_dx12Ref.commandList()->SetGraphicsRootDescriptorTable(2, materialH);
		m_dx12Ref.commandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}

}

void PMDActor::update() {
	m_angle += 0.03f;
	m_mappedTransform->world =  XMMatrixRotationY(m_angle);
}

// ---------------------------------------------------------------- //
//  privateメソッド実装
// ---------------------------------------------------------------- //

HRESULT PMDActor::createTransformView() {
	// GPUバッファ作成
	auto buffSize = sizeof(Transform);
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

	// マップとコピー
	result = m_transformBuff->Map(0, nullptr, (void**)&m_mappedTransform);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	*m_mappedTransform = m_transform;

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
			srvDesc.Format = m_rendererRef.m_whiteTex->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_rendererRef.m_whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = m_textureResources[i]->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_textureResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;
		
		// SPHテクスチャ用のSRV作成
		if (m_sphResources[i] == nullptr) {
			srvDesc.Format = m_rendererRef.m_whiteTex->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_rendererRef.m_whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = m_sphResources[i]->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_sphResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		// SPAテクスチャ用のSRV作成
		if (m_spaResources[i] == nullptr) {
			srvDesc.Format = m_rendererRef.m_blackTex->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_rendererRef.m_blackTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = m_spaResources[i]->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_spaResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		// Toonテクスチャ用のSRV作成
		if (m_toonResources[i] == nullptr) {
			srvDesc.Format = m_rendererRef.m_gradTex->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_rendererRef.m_gradTex.Get(), &srvDesc, matDescHeapH);
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
	fclose(fp);
}
