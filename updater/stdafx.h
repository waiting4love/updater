#pragma once
// Change these values to use different versions
#define WINVER		0x0601
#define _WIN32_WINNT	0x0601
#define _WIN32_IE	0x0700
#define _RICHEDIT_VER	0x0500

#include <Windows.h>

#include <iostream>
#include <clocale>
#include <vector>
#include <string>
#include <filesystem>
#include <sstream>
#include <functional>
#include <string_view>

#include "Application.h"
extern Application gApp;