#pragma once
#include <string>
std::wstring to_wstring(std::string_view s, UINT code_page = CP_UTF8);
std::wstring to_wstring(std::wstring s);
bool isEmptyOrSpace(std::string_view s);
bool isEmptyOrSpace(std::wstring_view s);
