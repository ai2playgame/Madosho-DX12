#include "PMDActor.hpp"
#include "PMDRenderer.hpp"
#include "Dx12Wrapper.hpp"
#include "PathOperator.hpp"
#include <d3dx12.h>

using namespace Microsoft::WRL;
using namespace DirectX;

// ---------------------------------------------------------------- //
//  �������O���
// ---------------------------------------------------------------- //
namespace {
	// PathOperator.hpp�Ɉڏ�
}

// ---------------------------------------------------------------- //
//  �����N���X����
// ---------------------------------------------------------------- //

void* PMDActor::Transform::operator new(size_t size) {
	return _aligned_malloc(size, 16);
}

// ---------------------------------------------------------------- //
//  public���\�b�h����
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
	// �}�e���A��
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
//  private���\�b�h����
// ---------------------------------------------------------------- //

HRESULT PMDActor::createTransformView() {
	// GPU�o�b�t�@�쐬
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

	// �}�b�v�ƃR�s�[
	result = m_transformBuff->Map(0, nullptr, (void**)&m_mappedTransform);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	*m_mappedTransform = m_transform;

	// �r���[�̍쐬
	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.NumDescriptors = 1; // �Ƃ肠�������[���h�ЂƂ�
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;

	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // �f�X�N���v�^�q�[�v���
	result = m_dx12Ref.device()->CreateDescriptorHeap(&transformDescHeapDesc,
		IID_PPV_ARGS(m_transformHeap.ReleaseAndGetAddressOf())); // ����
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
	// �}�e���A���o�b�t�@���쐬
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff)&~0xff;
	auto result = m_dx12Ref.device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize*m_materials.size()),// �ܑ̂Ȃ����ǎd���Ȃ��ł���
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_materialBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	// GPU��̃������Ƀ}�b�s���O
	char* mapMaterial = nullptr;
	result = m_materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	for (auto& m : m_materials) {
		*((MaterialForHlsl*)mapMaterial) = m.material; // �f�[�^�R�s�[
		mapMaterial += materialBuffSize; // ���̃A���C�����g�ʒu�܂Ői�߂�
	}
	m_materialBuff->Unmap(0, nullptr);

	return S_OK;

}

HRESULT PMDActor::createMaterialAndTextureView() {
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.NumDescriptors = m_materials.size() * 5; // �}�e���A�����Ԃ�(�萔1�A�e�N�X�`��3��)
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;

	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // �f�X�N���v�^�q�[�v���
	auto result = m_dx12Ref.device()->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(m_materialHeap.ReleaseAndGetAddressOf()));//����
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff)&~0xff;
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc{};
	matCBVDesc.BufferLocation = m_materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize;
	
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // TODO:��q
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2D�e�N�X�`��
	srvDesc.Texture2D.MipLevels = 1; // �~�b�v�}�b�v�͎g�p���Ȃ��̂�1
	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH(m_materialHeap->GetCPUDescriptorHandleForHeapStart());
	auto incSize = m_dx12Ref.device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (int i = 0; i < m_materials.size(); ++i) {
		// �}�e���A���Œ�o�b�t�@�r���[
		m_dx12Ref.device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;
		if (m_textureResources[i] == nullptr) {
			srvDesc.Format = m_rendererRef.m_whiteTex->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_rendererRef.m_whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = m_textureResources[i]->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_textureResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.Offset(incSize);

		if (m_sphResources[i] == nullptr) {
			srvDesc.Format = m_rendererRef.m_whiteTex->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_rendererRef.m_whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = m_sphResources[i]->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_sphResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		if (m_spaResources[i] == nullptr) {
			srvDesc.Format = m_rendererRef.m_blackTex->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_rendererRef.m_blackTex.Get(), &srvDesc, matDescHeapH);
		}
		else {
			srvDesc.Format = m_spaResources[i]->GetDesc().Format;
			m_dx12Ref.device()->CreateShaderResourceView(m_spaResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;


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
	// PMD�w�b�_�\����
	struct PMDHeader {
		float version; // ��F00 00 80 3F == 1.00
		char model_name[20]; // ���f����
		char comment[256]; // ���f���R�����g
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

	unsigned int vertNum; // ���_��
	fread(&vertNum, sizeof(vertNum), 1, fp);


#pragma pack(1)// ��������1�o�C�g�p�b�L���O�c�A���C�����g�͔������Ȃ�
	// PMD�}�e���A���\����
	struct PMDMaterial {
		XMFLOAT3 diffuse; // �f�B�t���[�Y�F
		float alpha; // �f�B�t���[�Y��
		float specularity;// �X�y�L�����̋���(��Z�l)
		XMFLOAT3 specular; // �X�y�L�����F
		XMFLOAT3 ambient; // �A���r�G���g�F
		unsigned char toonIdx; // �g�D�[���ԍ�(��q)
		unsigned char edgeFlg;// �}�e���A�����̗֊s���t���O
		// 2�o�C�g�̃p�f�B���O�������I�I
		unsigned int indicesNum; // ���̃}�e���A�������蓖����C���f�b�N�X��
		char texFilePath[20]; // �e�N�X�`���t�@�C����(�v���X�A���t�@�c��q)
	}; // 70�o�C�g�̂͂��c�ł��p�f�B���O���������邽��72�o�C�g
#pragma pack() // 1�o�C�g�p�b�L���O����

	constexpr unsigned int pmdvertex_size = 38; // ���_1������̃T�C�Y
	std::vector<unsigned char> vertices(vertNum*pmdvertex_size); // �o�b�t�@�m��
	fread(vertices.data(), vertices.size(), 1, fp); // ��C�ɓǂݍ���

	unsigned int indicesNum; // �C���f�b�N�X��
	fread(&indicesNum, sizeof(indicesNum), 1, fp);

	// UPLOAD(�m�ۂ͉\)
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


	m_vbView.BufferLocation = m_vb->GetGPUVirtualAddress(); // �o�b�t�@�̉��z�A�h���X
	m_vbView.SizeInBytes = vertices.size(); // �S�o�C�g��
	m_vbView.StrideInBytes = pmdvertex_size; // 1���_������̃o�C�g��

	std::vector<unsigned short> indices(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp); // ��C�ɓǂݍ���


	// �ݒ�́A�o�b�t�@�̃T�C�Y�ȊO���_�o�b�t�@�̐ݒ���g���܂킵��OK?
	result = m_dx12Ref.device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0])),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_ib.ReleaseAndGetAddressOf()));

	// ������o�b�t�@�ɃC���f�b�N�X�f�[�^���R�s�[
	unsigned short* mappedIdx = nullptr;
	m_ib->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(indices.begin(), indices.end(), mappedIdx);
	m_ib->Unmap(0, nullptr);


	// �C���f�b�N�X�o�b�t�@�r���[���쐬
	m_ibView.BufferLocation = m_ib->GetGPUVirtualAddress();
	m_ibView.Format = DXGI_FORMAT_R16_UINT;
	m_ibView.SizeInBytes = indices.size() * sizeof(indices[0]);

	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);
	m_materials.resize(materialNum);
	m_textureResources.resize(materialNum);
	m_sphResources.resize(materialNum);
	m_spaResources.resize(materialNum);
	m_toonResources.resize(materialNum);

	std::vector<PMDMaterial> pmdMaterials(materialNum);
	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);
	// �R�s�[
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
		// �g�D�[�����\�[�X�̓ǂݍ���
		char toonFilePath[32];
		sprintf(toonFilePath, "toon/toon%02d.bmp", pmdMaterials[i].toonIdx + 1);
		m_toonResources[i] = m_dx12Ref.getTextureByPath(toonFilePath);

		if (strlen(pmdMaterials[i].texFilePath) == 0) {
			m_textureResources[i] = nullptr;
			continue;
		}

		std::string texFileName = pmdMaterials[i].texFilePath;
		std::string sphFileName = "";
		std::string spaFileName = "";
		if (count(texFileName.begin(), texFileName.end(), '*') > 0) {//�X�v���b�^������
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
		// ���f���ƃe�N�X�`���p�X����A�v���P�[�V��������̃e�N�X�`���p�X�𓾂�
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

