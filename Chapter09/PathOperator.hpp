#pragma once
#include <string>
#include <algorithm>
#include <tuple>
#include <cassert>
#include <Windows.h>

class PathOperator {
private:
	PathOperator() = delete;

public:
	/// <summary>
	/// ���f���̃p�X�ƃe�N�X�`���̃p�X��^�������p�X�𓾂�
	/// </summary>
	/// <param name="modelPath">�A�v���P�[�V�������猩��PMD���f���̑��΃p�X</param>
	/// <param name="texPath">PMD���f�����猩���e�N�X�`���̑��΃p�X</param>
	/// <returns>�A�v���P�[�V�������猩���e�N�X�`���̑��΃p�X</returns>
    static std::string getTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath) {
		// �t�@�C���̃t�H���_��؂��\��/�̓��ނ��g�p�����\��������
		// �Ƃ�����������\��/�𓾂���΂����̂ŁA�o����rfind���Ƃ��r����
		// int�^�ɑ�����Ă���̂͌�����Ȃ������ꍇ��rfind��epos(-1��0xffffffff)��Ԃ�����
		int pathIndex1 = modelPath.rfind('/');
		int pathIndex2 = modelPath.rfind('\\');
		auto pathIndex = (std::max)(pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr(0, pathIndex + 1);
		return folderPath + texPath;
	}

	/// <summary>
	/// �t�@�C��������g���q���擾
	/// </summary>
	/// <param name="path">�Ώۂ̃p�X������</param>
	/// <returns>�g���q</returns>
	static std::string getExtension(const std::string& path) {
		auto idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	/// <summary>
	/// �e�N�X�`���̃p�X���Z�p���[�^�����ŕ���
	/// </summary>
	/// <param name="path">�Ώۂ̃p�X������</param>
	/// <param name="separator">��؂蕶��</param>
	/// <returns>��������������y�A</returns>
	static std::tuple<std::string, std::string> splitFileName(std::string& path, const char separator = '*') {
		auto idx = path.find(separator);
		return { path.substr(0, idx), path.substr(idx + 1, path.length() - idx - 1) };
	}

	/// <summary>
	/// string�i�}���`�o�C�g������j����wstring�i���C�h������j�𓾂�
	/// </summary>
	/// <param name="str">�}���`�o�C�g������</param>
	/// <returns>�ϊ����ꂽ���C�h������</returns>
	static std::wstring toWString(const std::string& str) {
		auto num1 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
		std::wstring wstr;
		wstr.resize(num1);
		auto num2 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, &wstr[0], num1);
		assert(num1 == num2);
		return wstr;
	}
};
