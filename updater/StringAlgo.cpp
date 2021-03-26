#include "stdafx.h"
#include "StringAlgo.h"
#include <algorithm>

std::wstring to_wstring(std::wstring s)
{
	return s;
}

std::wstring to_wstring(std::string_view s, UINT code_page)
{
	int len = ::MultiByteToWideChar(code_page, 0, s.data(), s.length(), NULL, 0);
	std::wstring ws(len, L'\0');
	::MultiByteToWideChar(code_page, 0, s.data(), s.length(), ws.data(), len);
	return ws;
}

std::string to_string(std::string s)
{
	return s;
}

std::string to_string(std::wstring_view ws, UINT code_page)
{
	int len = ::WideCharToMultiByte(code_page, 0, ws.data(), ws.length(), NULL, 0, NULL, NULL);
	std::string s(len, '\0');
	::WideCharToMultiByte(code_page, 0, ws.data(), ws.length(), s.data(), len, NULL, NULL);
	return s;
}

bool isEmptyOrSpace(std::string_view s)
{
	return std::all_of(s.begin(), s.end(), [](char c)->bool {return std::isspace(c); });
}

bool isEmptyOrSpace(std::wstring_view s)
{
	return std::all_of(s.begin(), s.end(), [](wchar_t c)->bool {return std::isspace(c); });
}