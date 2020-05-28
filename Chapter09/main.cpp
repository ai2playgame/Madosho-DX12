#include "Application.hpp"

#ifndef _DEBUG
int main() {
#else
#include <Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif
	auto& app = Application::instance();
	if (!app.init()){
		return -1;
	}
	app.run();
	app.terminate();
	return 0;
}
