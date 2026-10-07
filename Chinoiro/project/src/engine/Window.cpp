
#include "Window.h"
#include <cstdint>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#pragma comment(lib, "winmm.lib")

#ifdef USE_IMGUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

void Window::Initialize() {
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	//WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc; //ウィンドウプロシージャ
	wc.lpszClassName = L"CG2WindowClass"; //ウィンドウクラス名
	wc.hInstance = GetModuleHandle(nullptr); //インスタンスハンドル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW); //カーソル
	RegisterClass(&wc); //ウィンドウクラスを登録する
	timeBeginPeriod(1);//システムタイマーの分解能を上げる

	//ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0, kClientWidth, kClientHeight };

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	 hwnd = CreateWindow(
		wc.lpszClassName,
		L"chinoiro",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr);

	ShowWindow(hwnd, SW_SHOW);
}

bool Window::ProcessMessage() {
	MSG msg{};

	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (msg.message == WM_QUIT) {
		return true;
	}

	return false;
}

void Window::Finalize() {
	CloseWindow(hwnd);
	CoUninitialize();
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif

	//メッセージに応じてゲーム固有の処理を行う
	switch (msg) {

		//ウィンドウが破棄された
	case  WM_DESTROY:
		//OSに対してアプリの終了を伝える
		PostQuitMessage(0);

		return 0;
	}
	//標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}
