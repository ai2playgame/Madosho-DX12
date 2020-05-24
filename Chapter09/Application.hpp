#pragma once
#include<Windows.h>
#include<tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXMath.h>
#include<vector>
#include<map>
#include<d3dcompiler.h>
#include<DirectXTex.h>
#include<d3dx12.h>
#include<wrl.h>
#include<memory>

// 前方宣言
class DX12Wrapper;
class PMDRenderer;
class PMDActor;

// シングルトンクラス
class Application
{
	// ---------------------------------------------------------------- //
	//	シングルトン用設定
	// ---------------------------------------------------------------- //
private:
	// コンストラクタをprivateで宣言
	// コピーと代入を禁止に
	Application() = default;
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

public:
	~Application() = default;
	// Applicationのシングルトンインスタンスを得る
	static Application& instance();

	// ---------------------------------------------------------------- //
	//	publicメソッド
	// ---------------------------------------------------------------- //

	// 初期化
	bool init();

	// ループ起動
	void run();

	// 後処理
	void terminate();

	// ウィンドウサイズ取得
	SIZE getWindowSize() const;

	//ゲーム用ウィンドウの生成
	void createGameWindow(HWND &hwnd, WNDCLASSEX &windowClass);

	// ---------------------------------------------------------------- //
	//	publicフィールド
	// ---------------------------------------------------------------- //

	//ウィンドウ周り
	static const unsigned int WIN_WIDTH;
	static const unsigned int WIN_HEIGHT;
	WNDCLASSEX m_windowClass;
	HWND m_hwnd;

	// DX12描画ラッパ
	std::shared_ptr<DX12Wrapper> m_dx12;
	
	// PMDレンダラ＆アクター
	std::shared_ptr<PMDRenderer> m_pmdRenderer;
	std::shared_ptr<PMDActor> m_pmdActor;
};
