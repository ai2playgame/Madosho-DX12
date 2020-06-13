#include "Application.hpp"
#include "DX12Wrapper.hpp"
#include "PMDRenderer.hpp"
#include "PMDActor.hpp"

// ---------------------------------------------------------------- //
//	static 定数メンバの定義
// ---------------------------------------------------------------- //
const unsigned int Application::WIN_WIDTH = 1280;
const unsigned int Application::WIN_HEIGHT = 720;

// ---------------------------------------------------------------- //
//	無名名前空間
// ---------------------------------------------------------------- //

namespace {
// ウィンドウプロシージャ（WinAPI）
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) {
		PostQuitMessage(0); // アプリケーション終了をOSに伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);	// 既定の処理
}
}

// ---------------------------------------------------------------- //
//	publicメソッド
// ---------------------------------------------------------------- //

Application& Application::instance() {
	static Application instance;
	return instance;
}

bool Application::init() {
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	createGameWindow(m_hwnd, m_windowClass);

	// DirectX12ラッパオブジェクト生成
	m_dx12.reset(new DX12Wrapper(m_hwnd));
	m_pmdRenderer.reset(new PMDRenderer(*m_dx12));
	
	using ActorPtr = std::shared_ptr<PMDActor>;

	ActorPtr miku = std::make_shared<PMDActor>("Model/初音ミク.pmd", *m_dx12);
	miku->move(0, 0, 0);
	miku->loadVMDFile("motion/motion.vmd");
	m_pmdRenderer->addActor(miku);

	ActorPtr ruka = std::make_shared<PMDActor>("Model/巡音ルカ.pmd", *m_dx12);
	ruka->move(-15, 0, 0);
	ruka->loadVMDFile("motion/motion.vmd");
	m_pmdRenderer->addActor(ruka);

	ActorPtr rin = std::make_shared<PMDActor>("Model/鏡音リン.pmd", *m_dx12);
	rin->move(15, 0, 0);
	rin->loadVMDFile("motion/motion.vmd");
	m_pmdRenderer->addActor(rin);

	m_pmdRenderer->beginAnimation();

	return true;
}

void Application::run() {
	ShowWindow(m_hwnd, SW_SHOW);

	float angle = 0.f;
	MSG msg{};
	unsigned int frame = 0;
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (msg.message == WM_QUIT) {
			break;
		}

		m_dx12->beginDraw();
		m_dx12->commandList()->SetPipelineState(m_pmdRenderer->pipelineState());
		m_dx12->commandList()->SetGraphicsRootSignature(m_pmdRenderer->rootSignature());
		m_dx12->commandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_dx12->setScene();

		m_pmdRenderer->update();
		m_pmdRenderer->draw();

		m_dx12->endDraw();
		m_dx12->swapchain()->Present(1, 0);
	}
}

void Application::terminate() {
	UnregisterClass(m_windowClass.lpszClassName, m_windowClass.hInstance);
}

SIZE Application::getWindowSize() const {
	SIZE ret;
	ret.cx = WIN_WIDTH;
	ret.cy = WIN_HEIGHT;

	return ret;
}

void Application::createGameWindow(HWND& hwnd, WNDCLASSEX& windowClass) {
	HINSTANCE hInst = GetModuleHandle(nullptr);
	// ウィンドウクラス生成＆登録
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック指定
	windowClass.lpszClassName = _T("DirectX12 Test");
	windowClass.hInstance = GetModuleHandle(0);
	RegisterClassEx(&windowClass); // アプリケーションに設定

	RECT wrc = { 0, 0, WIN_WIDTH, WIN_HEIGHT };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // ウィンドウサイズ補正
	
	// 実際にウィンドウオブジェクトを生成する
	hwnd = CreateWindow(windowClass.lpszClassName,
		_T("DX12 Refactor"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		windowClass.hInstance,
		nullptr);
}

// ---------------------------------------------------------------- //
//	privateメソッド
// ---------------------------------------------------------------- //
