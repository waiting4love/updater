#include "stdafx.h"
#include <Windows.h>
#include <Ole2.h>
#include <CommCtrl.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Rpcrt4.lib")

Application gApp;

int main()
{
	::CoInitialize(NULL);
	int ret = 0;
	gApp.init();
	try {
		ret = gApp.run();
	}
	catch (const std::exception& e) {
		gApp.err(-1, e.what());
	}
	::CoUninitialize();
	return ret;
}