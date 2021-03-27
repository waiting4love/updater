#pragma once
#include <string>
std::wstring to_wstring(std::string_view s, UINT code_page = CP_UTF8);
//std::wstring to_wstring(std::wstring s);
//std::string to_string(std::string s);
std::string to_string(std::wstring_view ws, UINT code_page = CP_UTF8);
bool isEmptyOrSpace(std::string_view s);
bool isEmptyOrSpace(std::wstring_view s);
