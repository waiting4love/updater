#include "pch.h"
#include "Exports.h"
#include "UpdateService.h"
#include <algorithm>
#include <numeric>
#include <memory>

std::unique_ptr<UpdateService> g_Service;

bool __stdcall Update_Initialize()
{
	g_Service = std::make_unique<UpdateService>();
	return true;
}
bool __stdcall Update_Uninitialize()
{
	g_Service.reset();
	return true;
}

bool __stdcall Update_IsAvailable()
{
	try {
		return g_Service->isAvailable();
	}
	catch (...) {
		return false;
	}
}
bool __stdcall Update_StartWatch(int checkIntervalMs, UpdateWatcher watcher, void* param)
{
	try {
		g_Service->setCheckInterval(checkIntervalMs);
		g_Service->setVersionReceivedHandler([watcher, param]() {watcher(param); });
		g_Service->start();
		return true;
	}
	catch (...) {
		return false;
	}

}
bool __stdcall Update_StopWatch()
{
	try {
		g_Service->stop();
		return true;
	}
	catch (...) {
		return false;
	}
}
bool __stdcall Update_Perform(bool restart)
{
	try {
		g_Service->setRestartAppFlag(restart);
		return g_Service->doUpdate();
	}
	catch (...) {
		return false;
	}
}
bool __stdcall Update_IsError()
{
	try {
		return g_Service->isError();
	}
	catch (...) {
		return false;
	}
}
bool __stdcall Update_IsNothing()
{
	try {
		return g_Service->IsNothing();
	}
	catch (...) {
		return false;
	}
}
VersionMessage __stdcall Update_GetVersionMessage()
{
	auto info = g_Service->getVersionInfo();
	auto res = new VersionInformation(std::move(info));
	return (VersionMessage)res;
}
bool __stdcall Update_IsNewVersionReady()
{
	try {
		return g_Service->isNewVersionReady();
	}
	catch (...) {
		return false;
	}
}
bool __stdcall Update_Wait(int timeoutMs)
{
	try {
		return g_Service->waitVersionInfo(timeoutMs);
	}
	catch (...) {
		return false;
	}
}
bool __stdcall VersionMessage_Destory(VersionMessage msg)
{
	auto vi = (VersionInformation*)msg;
	delete vi;
	return true;
}
bool __stdcall VersionMessage_IsError(VersionMessage msg)
{
	auto vi = (VersionInformation*)msg;
	return vi->isError();
}
bool __stdcall VersionMessage_IsNewVersionReady(VersionMessage msg)
{
	auto vi = (VersionInformation*)msg;
	return vi->isNewVersionReady();
}
bool __stdcall VersionMessage_IsNothing(VersionMessage msg)
{
	auto vi = (VersionInformation*)msg;
	return vi->isEmpty();
}
int copyStringList(const StringList& src, char* out, int len)
{
	int totalSize = std::reduce(src.begin(), src.end(), (size_t)0, [](size_t n, const String& s) { return n + s.length() + 1; });
	if (totalSize < len)
	{
		for (auto& s : src)
		{
			memcpy(out, s.data(), s.length());
			out += s.length();
			*(out++) = '\n';
		}
		*out = '\0';
	}
	return totalSize;
}
int __stdcall VersionMessage_GetRemoteMessage(VersionMessage msg, char* message, int len)
{
	auto vi = (VersionInformation*)msg;
	if (vi == nullptr) return 0;
	return copyStringList(vi->Status.Remote, message, len);
}
int __stdcall VersionMessage_GetLocalMessage(VersionMessage msg, char* message, int len)
{
	auto vi = (VersionInformation*)msg;
	if (vi == nullptr) return 0;
	return copyStringList(vi->Status.Local, message, len);
}
int __stdcall VersionMessage_GetWhatsNewMessage(VersionMessage msg, char* message, int len)
{
	auto vi = (VersionInformation*)msg;
	if (vi == nullptr) return 0;
	int totalSize = std::reduce(vi->Status.New.begin(), vi->Status.New.end(),
		(size_t)0, [](size_t n, const StringList& s) { return n + copyStringList(s, nullptr, 0) + 1; });
	if (totalSize < len)
	{
		for (const auto& s : vi->Status.New)
		{
			int item_len = copyStringList(s, message, len);
			len -= item_len + 1;
			message += item_len;
			*(message++) = '\0';
		}
		*message = '\0';
	}
	return totalSize;
}
int __stdcall VersionMessage_GetErrorMessage(VersionMessage msg, char* message, int len)
{
	auto vi = (VersionInformation*)msg;
	if (vi == nullptr) return 0;
	int msg_len = vi->ErrorMessage.length();
	if (msg_len < len)
	{
		memcpy(message, vi->ErrorMessage.data(), msg_len);
		message[msg_len] = '\0';
	}
	return msg_len;
}