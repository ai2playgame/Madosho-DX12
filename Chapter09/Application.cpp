#include "Application.hpp"
#include "DX12Wrapper.hpp"
#include "PMDRenderer.hpp"
#include "PMDActor.hpp"

// ---------------------------------------------------------------- //
//	static �萔�����o�̒�`
// ---------------------------------------------------------------- //
const unsigned int Application::WIN_WIDTH = 1280;
const unsigned int Application::WIN_HEIGHT = 720;

// ---------------------------------------------------------------- //
//	�������O���
// ---------------------------------------------------------------- //

namespace {
// �E�B���h�E�v���V�[�W���iWinAPI�j
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) {
		PostQuitMessage(0); // �A�v���P�[�V�����I����OS�ɓ`����
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);	// ����̏���
}
}

// ---------------------------------------------------------------- //
//	public���\�b�h
// ---------------------------------------------------------------- //

Application& Application::instance() {
	static Application instance;
	return instance;
}

bool Application::init() {
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	createGameWindow(m_hwnd, m_windowClass);

	// DirectX12���b�p�I�u�W�F�N�g����
	m_dx12.reset(new DX12Wrapper(m_hwnd));
	m_pmdRenderer.reset(new PMDRenderer(*m_dx12));
	m_pmdActor.reset(new PMDActor("Model/�����~�N.pmd", *m_pmdRenderer));

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
		m_dx12->commandList()->SetPipelineState(m_pmdRenderer->getPipelineState());
		m_dx12->commandList()->SetGraphicsRootSignature(m_pmdRenderer->getRootSignature());
		m_dx12->commandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_dx12->setScene();

		m_pmdActor->update();
		m_pmdActor->draw();

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
	// �E�B���h�E�N���X�������o�^
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure; // �R�[���o�b�N�w��
	windowClass.lpszClassName = _T("DirectX12 Test");
	windowClass.hInstance = GetModuleHandle(0);
	RegisterClassEx(&windowClass); // �A�v���P�[�V�����ɐݒ�

	RECT wrc = { 0, 0, WIN_WIDTH, WIN_HEIGHT };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // �E�B���h�E�T�C�Y�␳
	
	// ���ۂɃE�B���h�E�I�u�W�F�N�g�𐶐�����
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
//	private���\�b�h
// ---------------------------------------------------------------- //


