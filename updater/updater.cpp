#include "stdafx.h"
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Rpcrt4.lib")

Application gApp;

int main()
{
	gApp.init();
	try {
		gApp.run();
	}
	catch (const std::exception& e) {
		gApp.err(-1, e.what());
	}
	return 0;
}