#include "pch.h"
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

bool isEmptyOrSpace(std::string_view s)
{
	return std::all_of(s.begin(), s.end(), std::isspace);
}

bool isEmptyOrSpace(std::wstring_view s)
{
	return std::all_of(s.begin(), s.end(), std::isspace);
}