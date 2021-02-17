#include "pch.h"
#include "Exports.h"
#include "UpdateService.h"
#include "UpdateStaticText.h"
#include "StringAlgo.h"
#include <algorithm>
#include <numeric>
#include <memory>
#include <mutex>
#include <atldlgs.h>

class ExportHelper
{
private:
	struct Observer
	{
		HWND hwnd;
		HWND parent;
	};

	UpdateService service;
	std::mutex mutexForWatcher;
	std::vector<Observer> observers;
	UpdateReceivedEvent watcher{ nullptr };
	void* watcher_param{ nullptr };
	static std::unique_ptr<ExportHelper> instance;
public:
	ExportHelper()
	{
		service.setVersionReceivedHandler([this]() { OnVersionReceived(); });
	}
	static void Initialize()
	{
		if(!instance) instance = std::make_unique<ExportHelper>();
	}
	static void Uninitialize()
	{
		instance.reset();
	}
	[[nodiscard]]
	static ExportHelper& GetInstance()
	{
		if (!instance) Initialize();
		return *instance;
	}
	bool IsAvailable() const
	{
		try {
			return service.isAvailable();
		}
		catch (...) {
			return false;
		}
	}
	void OnVersionReceived()
	{
		UpdateReceivedEvent cur_watcher{ nullptr };
		void* cur_watcher_param{ nullptr };

		{
			std::scoped_lock sl{ mutexForWatcher };
			cur_watcher = watcher;
			cur_watcher_param = watcher_param;
			// remove invalid hwnd
			if (
				auto itr = std::remove_if(observers.begin(), observers.end(), [](auto& ob) {return ::IsWindow(ob.hwnd) == FALSE; });
				itr != observers.end()
				)
			{
				observers.erase(itr, observers.end());
			}

			std::for_each(
				observers.begin(), observers.end(),
				[](auto& ob) { ::PostMessage(ob.hwnd, UpdateStaticText::MSG_VERSIONINFO_RECEVICED, 0, 0); });
		}

		if (cur_watcher != nullptr)
		{
			cur_watcher(cur_watcher_param);
		}
	}
	bool StartWatch(int checkIntervalMs, UpdateReceivedEvent watcher, void* param)
	{
		try {
			std::scoped_lock sl{ mutexForWatcher };
			this->watcher = watcher;
			this->watcher_param = param;
			service.setCheckInterval(checkIntervalMs);
			service.start();
			return true;
		}
		catch (...) {
			return false;
		}
	}
	bool StopWatch()
	{
		try {
			service.stop();
			return true;
		}
		catch (...) {
			return false;
		}
	}
	bool Perform(bool restart)
	{
		try {
			service.setRestartAppFlag(restart);
			return service.doUpdate();
		}
		catch (...) {
			return false;
		}
	}
	bool IsError() const
	{
		try {
			return service.isError();
		}
		catch (...) {
			return false;
		}
	}
	bool IsNothing() const
	{
		try {
			return service.IsNothing();
		}
		catch (...) {
			return false;
		}
	}
	VersionMessage GetVersionMessage()
	{
		auto info = service.getVersionInfo();
		auto res = new VersionInformation(std::move(info));
		return (VersionMessage)res;
	}
	VersionMessage MoveVersionMessage()
	{
		auto info = service.moveVersionInfo();
		auto res = new VersionInformation(std::move(info));
		return (VersionMessage)res;
	}
	bool IsNewVersionReady() const
	{
		try {
			return service.isNewVersionReady();
		}
		catch (...) {
			return false;
		}
	}
	bool Wait(int timeoutMs)
	{
		try {
			return service.waitVersionInfo(timeoutMs);
		}
		catch (...) {
			return false;
		}
	}
	UpdateStaticText* CreateStaticText(HWND parent, LPRECT rect, UINT id)
	{
		auto* sta = new UpdateStaticText{};
		sta->Create(parent, rect, NULL, 0, 0, id);
		sta->SetWindowPos(HWND_TOP, rect, SWP_SHOWWINDOW);
		HWND hwnd = *sta;
		std::scoped_lock sl{ mutexForWatcher };
		observers.push_back({ hwnd, ::GetParent(hwnd) });
		return sta;
	}
};
std::unique_ptr<ExportHelper> ExportHelper::instance;

void __stdcall Update_Initialize()
{
	ExportHelper::Initialize();
}
void __stdcall Update_Uninitialize()
{
	ExportHelper::Uninitialize();
}

bool __stdcall Update_IsAvailable()
{
	return ExportHelper::GetInstance().IsAvailable();
}
bool __stdcall Update_StartWatch(int checkIntervalMs, UpdateReceivedEvent watcher, void* param)
{
	return ExportHelper::GetInstance().StartWatch(checkIntervalMs, watcher, param);
}
bool __stdcall Update_StopWatch()
{
	return ExportHelper::GetInstance().StopWatch();
}
bool __stdcall Update_Perform(bool restart)
{
	return ExportHelper::GetInstance().Perform(restart);
}
bool __stdcall Update_IsError()
{
	return ExportHelper::GetInstance().IsError();
}
bool __stdcall Update_IsNothing()
{
	return ExportHelper::GetInstance().IsNothing();
}
VersionMessage __stdcall Update_GetVersionMessage()
{
	return ExportHelper::GetInstance().GetVersionMessage();
}
VersionMessage __stdcall Update_MoveVersionMessage()
{
	return ExportHelper::GetInstance().MoveVersionMessage();
}
bool __stdcall Update_IsNewVersionReady()
{
	return ExportHelper::GetInstance().IsNewVersionReady();
}
bool __stdcall Update_Wait(int timeoutMs)
{
	return ExportHelper::GetInstance().Wait(timeoutMs);
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
	return vi? vi->isError() : false;
}
bool __stdcall VersionMessage_IsNewVersionReady(VersionMessage msg)
{
	auto vi = (VersionInformation*)msg;
	return vi ? vi->isNewVersionReady() : false;
}
bool __stdcall VersionMessage_IsNothing(VersionMessage msg)
{
	auto vi = (VersionInformation*)msg;
	return vi? vi->isEmpty() : true;
}
int copyStringList(const StringList& src, wchar_t* out, int len)
{
	int totalSize = std::reduce(src.begin(), src.end(), (size_t)0, [](size_t n, const String& s) { return n + s.length() + 1; });
	if (totalSize < len)
	{
		for (auto& s : src)
		{
			memcpy(out, s.data(), s.length()*sizeof(*out));
			out += s.length();
			*(out++) = '\n';
		}
		*out = '\0';
	}
	return totalSize+1;
}
int __stdcall VersionMessage_GetRemoteMessage(VersionMessage msg, wchar_t* message, int len)
{
	auto vi = (VersionInformation*)msg;
	if (vi == nullptr) return 0;
	return copyStringList(vi->Status.Remote, message, len);
}
int __stdcall VersionMessage_GetLocalMessage(VersionMessage msg, wchar_t* message, int len)
{
	auto vi = (VersionInformation*)msg;
	if (vi == nullptr) return 0;
	return copyStringList(vi->Status.Local, message, len);
}
int __stdcall VersionMessage_GetWhatsNewMessage(VersionMessage msg, wchar_t* message, int len)
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
	return totalSize+1;
}
int __stdcall VersionMessage_GetErrorMessage(VersionMessage msg, wchar_t* message, int len)
{
	auto vi = (VersionInformation*)msg;
	if (vi == nullptr) return 0;
	int msg_len = vi->ErrorMessage.length();
	if (msg_len < len)
	{
		lstrcpynW(message, vi->ErrorMessage.data(), len);
	}
	return msg_len+1;
}

VersionMessageLabel __stdcall VersionMessageLabel_Create(HWND parent, LPRECT rect, UINT id, bool manage_update_instance)
{
	auto lbl = ExportHelper::GetInstance().CreateStaticText(parent, rect, id);
	lbl->EnableManageUpdateInstance(manage_update_instance);
	if (manage_update_instance)
	{
		Update_Initialize();
		Update_StartWatch(60'000, nullptr, nullptr);
	}
	return (VersionMessageLabel)lbl;
}

void __stdcall VersionMessageLabel_Destory(VersionMessageLabel label) // it is not necessary
{
	auto sta = (UpdateStaticText*)label;
	delete sta;
}

HWND __stdcall VersionMessageLabel_GetHandle(VersionMessageLabel label)
{
	auto sta = (UpdateStaticText*)label;
	return *sta;
}
VersionMessage __stdcall VersionMessageLabel_GetVersionMessageRef(VersionMessageLabel label)
{
	auto sta = (UpdateStaticText*)label;
	return sta->GetLatestMessage();
}

void __stdcall VersionMessageLabel_SetShowingLabelEvent(VersionMessageLabel label, ShowingLabelEvent func, void* param)
{
	auto sta = (UpdateStaticText*)label;
	if (func)
	{
		sta->ShowingHandler = [label, func, param]() -> std::wstring {
			wchar_t text[MAX_LABLE_LEN] = { 0 };
			func(label, param, text);
			return text;
		};
	}
	else
	{
		sta->ShowingHandler = decltype(sta->ShowingHandler){};
	}
}

std::wstring to_content(const StringList& sl, bool skipEmpty = false)
{
	std::wstring ws;
	int idx = 0;
	for (auto& s : sl)
	{
		if (idx == 0)
		{
			ws = s;
		}
		else
		{
			if (isEmptyOrSpace(s))
			{
				if (skipEmpty)
					continue;
				else
					ws += _T("\n");
			}
			else
			{
				ws += _T("\n-  ") + to_wstring(s);
			}
		}
		idx++;
	}
	return ws;
}

int __stdcall VersionMessage_ShowBox(VersionMessage msg, HWND parent, const wchar_t* prompt, const wchar_t* buttons[], int buttons_size)
{
	auto vi = (VersionInformation*)msg;
	std::wstring contentText;
	std::wstring instructionText;

	CTaskDialog taskDlg;
	taskDlg.SetWindowTitle(_T("Check for Updates"));
	if (vi->isEmpty())
	{
		taskDlg.SetMainIcon(TD_WARNING_ICON);
		instructionText = _T("No Version Information Yet");
	}
	else if (vi->isError())
	{
		taskDlg.SetMainIcon(TD_ERROR_ICON);
		instructionText = _T("An Error Occurs");
		contentText = to_wstring(vi->ErrorMessage);
	}
	else if (vi->isNewVersionReady())
	{
		taskDlg.SetMainIcon(TD_INFORMATION_ICON);
		instructionText = _T("A New Version Is Available");
		if (vi->Status.New.empty())
		{
			contentText = to_content(vi->Status.Remote);
		}
		else
		{
			int idx = 0;
			for (const auto& sl : vi->Status.New)
			{
				auto s = to_content(sl, true);
				if (idx == 0)
					contentText = std::move(s);
				else
					contentText += _T("\n\n") + std::move(s);
				idx++;
				if (idx >= 3) break;
			}
		}
	}
	else
	{
		// current is latest
		taskDlg.SetMainIcon(TD_INFORMATION_ICON);
		instructionText = _T("Current is latest version");
		contentText = to_content(vi->Status.Local);
	}

	taskDlg.SetMainInstructionText(instructionText.c_str());
	taskDlg.SetContentText(contentText.c_str());
	taskDlg.SetCommonButtons(TDCBF_CLOSE_BUTTON);
	
	std::vector<TASKDIALOG_BUTTON> commandButtons;
	if (vi->isNewVersionReady() && buttons != nullptr && buttons_size > 0)
	{
		for(int i=0; i<std::min<>(buttons_size, 5); i++)
		{
			commandButtons.push_back({i+1, buttons[i] });
		}
		taskDlg.SetButtons(commandButtons.data(), commandButtons.size());
		taskDlg.ModifyFlags(0, TDF_USE_COMMAND_LINKS);
	}

	if(prompt != nullptr) taskDlg.SetFooterText(prompt);

	int nButtonPressed = 0;
	taskDlg.DoModal(parent, &nButtonPressed);
	return nButtonPressed;
}

void __stdcall VersionMessageLabel_EnableShowBoxOnClick(VersionMessageLabel label, bool enable, UnargEvent request_exit, void* param)
{
	auto sta = (UpdateStaticText*)label;
	if (request_exit)
	{
		sta->EnableShowBoxOnClick(enable, [request_exit, param]() {request_exit(param); });
	}
	else
	{
		sta->EnableShowBoxOnClick(enable, UpdateService::VersionReceivedHandler{});
	}
}

void __stdcall VersionMessageLabel_EnableAutoSize(VersionMessageLabel label,bool enable)
{
	auto sta = (UpdateStaticText*)label;
	sta->EnableAutoSize(enable);
}
void __stdcall VersionMessageLabel_EnablePerformUpdateOnExit(VersionMessageLabel label, bool enable)
{
	auto sta = (UpdateStaticText*)label;
	sta->EnablePerformUpdateOnExit(enable);
}

void __stdcall VersionMessageLabel_SetAlignment(VersionMessageLabel label, bool RightAlign, bool BottomAlign)
{
	auto sta = (UpdateStaticText*)label;
	sta->SetAlignment(
		RightAlign? UpdateStaticText::Align::Far : UpdateStaticText::Align::Near,
		BottomAlign ? UpdateStaticText::Align::Far : UpdateStaticText::Align::Near
		);
}

void __stdcall VersionMessageLabel_SetColor(VersionMessageLabel label, COLORREF color)
{
	auto sta = (UpdateStaticText*)label;
	sta->SetColor(color);
}

void __stdcall VersionMessageLabel_SetFont(VersionMessageLabel label, int nPointSize, LPCWSTR lpszFaceName)
{
	auto sta = (UpdateStaticText*)label;
	sta->SetFont(nPointSize, lpszFaceName);
}