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
	/// モデルのパスとテクスチャのパスを与え合成パスを得る
	/// </summary>
	/// <param name="modelPath">アプリケーションから見たPMDモデルの相対パス</param>
	/// <param name="texPath">PMDモデルから見たテクスチャの相対パス</param>
	/// <returns>アプリケーションから見たテクスチャの相対パス</returns>
    static std::string getTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath) {
		// ファイルのフォルダ区切りは\と/の二種類が使用される可能性があり
		// ともかく末尾の\か/を得られればいいので、双方のrfindをとり比較する
		// int型に代入しているのは見つからなかった場合はrfindがepos(-1→0xffffffff)を返すため
		auto pathIndex1 = static_cast<int>(modelPath.rfind('/'));
		auto pathIndex2 = static_cast<int>(modelPath.rfind('\\'));
		int pathIndex = (std::max)(pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr(0, static_cast<size_t>(pathIndex) + 1);
		return folderPath + texPath;
	}

	/// <summary>
	/// ファイル名から拡張子を取得
	/// </summary>
	/// <param name="path">対象のパス文字列</param>
	/// <returns>拡張子</returns>
	static std::string getExtension(const std::string& path) {
		auto idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	/// <summary>
	/// テクスチャのパスをセパレータ文字で分離
	/// </summary>
	/// <param name="path">対象のパス文字列</param>
	/// <param name="separator">区切り文字</param>
	/// <returns>分割した文字列ペア</returns>
	static std::tuple<std::string, std::string> splitFileName(std::string& path, const char separator = '*') {
		auto idx = path.find(separator);
		return { path.substr(0, idx), path.substr(idx + 1, path.length() - idx - 1) };
	}

	/// <summary>
	/// string（マルチバイト文字列）からwstring（ワイド文字列）を得る
	/// </summary>
	/// <param name="str">マルチバイト文字列</param>
	/// <returns>変換されたワイド文字列</returns>
	static std::wstring toWString(const std::string& str) {
		auto num1 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
		std::wstring wstr;
		wstr.resize(num1);
		auto num2 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, &wstr[0], num1);
		assert(num1 == num2);
		return wstr;
	}
};
