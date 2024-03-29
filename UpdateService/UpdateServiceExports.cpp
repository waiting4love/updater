#include "pch.h"
#include "UpdateServiceExports.h"
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
	bool IsAvailable() const noexcept
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
				[](auto& ob) { ::PostMessage(ob.hwnd, MSG_VERSIONINFO_RECEVICED, 0, 0); });
		}

		if (cur_watcher != nullptr)
		{
			cur_watcher(cur_watcher_param);
		}
	}
	void SetGuiFetch(bool gui)
	{
		service.setGuiFetch(gui);
	}
	bool StartWatch() noexcept
	{
		try {
			std::scoped_lock sl{ mutexForWatcher };
			service.start();
			return true;
		}
		catch (...) {
			return false;
		}
	}
	void SetWatcher(UpdateReceivedEvent watcher, void* param)
	{
		std::scoped_lock sl{ mutexForWatcher };
		this->watcher = watcher;
		this->watcher_param = param;
	}

	bool StopWatch() noexcept
	{
		try {
			service.stop();
			return true;
		}
		catch (...) {
			return false;
		}
	}
	bool SetCheckInterval(int intervalMs) noexcept
	{
		try {
			service.setCheckInterval(intervalMs);
			return true;
		}
		catch (...) {
			return false;
		}
	}
	bool Perform(bool restart, const wchar_t* extra_args) noexcept
	{
		try {
			service.setRestartAppFlag(restart, extra_args);
			return service.doUpdate();
		}
		catch (...) {
			return false;
		}
	}
	bool IsError() const noexcept
	{
		try {
			return service.isError();
		}
		catch (...) {
			return false;
		}
	}
	bool IsNothing() const noexcept
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
	bool IsNewVersionReady() const noexcept
	{
		try {
			return service.isNewVersionReady();
		}
		catch (...) {
			return false;
		}
	}
	bool Wait(int timeoutMs) noexcept
	{
		try {
			return service.waitVersionInfo(timeoutMs);
		}
		catch (...) {
			return false;
		}
	}
	UpdateStaticText* CreateStaticText(HWND parent, LPRECT rect, UINT id, DWORD style=0, DWORD ex_style = 0)
	{
		auto* sta = new UpdateStaticText{};
		sta->Create(parent, rect, nullptr, style, ex_style, id);
		//sta->SetWindowPos(HWND_TOP, rect, SWP_SHOWWINDOW);
		HWND hwnd = *sta;
		std::scoped_lock sl{ mutexForWatcher };
		observers.push_back({ hwnd, ::GetParent(hwnd) });
		return sta;
	}

	UpdateTextWin* CreateTextWin(HWND parent)
	{
		auto* sta = new UpdateTextWin{};
		sta->Create(parent);
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
void __stdcall Update_SetGuiFetch(bool guiFetch)
{
	ExportHelper::GetInstance().SetGuiFetch(guiFetch);
}
bool __stdcall Update_IsAvailable()
{
	return ExportHelper::GetInstance().IsAvailable();
}
bool __stdcall Update_StartWatch()
{
	return ExportHelper::GetInstance().StartWatch();
}
bool __stdcall Update_StopWatch()
{
	return ExportHelper::GetInstance().StopWatch();
}
bool __stdcall Update_SetCheckInterval(int intervalMs)
{
	return ExportHelper::GetInstance().SetCheckInterval(intervalMs);
}
bool __stdcall Update_SetWatcher(UpdateReceivedEvent watcher, void* param)
{
	ExportHelper::GetInstance().SetWatcher(watcher, param);
	return true;
}
bool __stdcall Update_Perform(bool restart, const wchar_t* extra_args)
{
	return ExportHelper::GetInstance().Perform(restart, extra_args);
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
bool __stdcall Update_EnsureUpdateOnAppEntry(int timeoutMs, bool showProgressBar) // true if request restart
{
	auto & inst = ExportHelper::GetInstance();
	inst.SetGuiFetch(showProgressBar);
	inst.StartWatch();
	if (inst.Wait(timeoutMs) && inst.IsNewVersionReady())
	{
		inst.StopWatch();
		inst.Perform(true, ::GetCommandLineW());
		return true;
	}
	Update_Uninitialize(); // it will be created again if necessary
	return false;	
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
	size_t totalSize = std::reduce(src.begin(), src.end(), (size_t)0, [](size_t n, const String& s) { return n + s.length() + 1; });
	if ((int)totalSize < len)
	{
		for (auto& s : src)
		{
			memcpy(out, s.data(), s.length()*sizeof(*out));
			out += s.length();
			*(out++) = '\n';
		}
		*out = '\0';
	}
	return (int)totalSize+1;
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
	size_t totalSize = std::reduce(vi->Status.New.begin(), vi->Status.New.end(),
		(size_t)0, [](size_t n, const StringList& s) { return n + copyStringList(s, nullptr, 0) + 1; });
	if ((int)totalSize < len)
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
	return (int)totalSize+1;
}
int __stdcall VersionMessage_GetErrorMessage(VersionMessage msg, wchar_t* message, int len)
{
	auto vi = (VersionInformation*)msg;
	if (vi == nullptr) return 0;
	auto msg_len = vi->ErrorMessage.length();
	if ((int)msg_len < len)
	{
		lstrcpynW(message, vi->ErrorMessage.data(), len);
	}
	return (int)msg_len+1;
}

VersionMessageLabel __stdcall VersionMessageLabel_Create(HWND parent, LPRECT rect, UINT id, bool manage_update_instance)
{
	auto lbl = ExportHelper::GetInstance().CreateStaticText(parent, rect, id);
	lbl->EnableManageUpdateInstance(manage_update_instance);
	if (manage_update_instance)
	{
		Update_Initialize();
		Update_StartWatch();
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

std::wstring GetShortText(std::wstring s, size_t max_len)
{
	const wchar_t trim_chars[] = L". \t";
	s.erase(0, s.find_first_not_of(trim_chars));
	s.erase(s.find_last_not_of(trim_chars) + 1);
	if (s.length() > max_len)
	{
		s = s.substr(0, max_len) + L"...";
	}
	return s;
}

std::wstring GetSingleLineText(VersionMessage msg)
{
	auto vi = (VersionInformation*)msg;
	const size_t max_len = 64;
	if (vi->isError())
	{		
		//return GetShortText(vi->ErrorMessage, max_len);
		if(vi->Status.Local.empty()) return L"An error occurred";
	}
	else if (vi->isNewVersionReady())
	{
		return GetShortText(vi->Status.Remote[0], max_len);
	}
	else if (VersionMessage_IsNothing(msg))
	{
		return L"Connecting...";
	}
	else if (vi->Status.Local.empty())
	{
		return L"Unrecognized status";
	}

	return GetShortText(vi->Status.Local[0], max_len);
}

std::wstring GetAllCaseText(VersionMessage msg)
{
	if (Update_IsAvailable())
	{
		auto ws = GetSingleLineText(msg);
		if (VersionMessage_IsNewVersionReady(msg))
		{
			ws += L" is ready!";
		}
		return ws;
	}
	return {};
}

void __stdcall VersionMessageLabel_SetShowingHandler(VersionMessageLabel label, ShowingLabelEvent func, void* param)
{
	auto sta = (UpdateStaticText*)label;
	if (func)
	{
		if (func == LABEL_TEXT_ALLCASE)
		{
			sta->SetShowingHandler([label](){
				auto msg = VersionMessageLabel_GetVersionMessageRef(label);
				return GetAllCaseText(msg);
			});
		}
		else
		{
			sta->SetShowingHandler([label, func, param](){
				std::wstring ws(MAX_LABLE_LEN, L'\0');
				auto msg = VersionMessageLabel_GetVersionMessageRef(label);
				func(msg, param, ws.data());
				return ws;
			});
		}
	}
	else
	{
		sta->SetShowingHandler({});
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
				ws += _T("\n-  ") + s;
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
		contentText = vi->ErrorMessage;
	}
	else if (vi->isNewVersionReady())
	{
		taskDlg.SetMainIcon(TD_INFORMATION_ICON);
		instructionText = vi->Status.Remote[0] + L" Is Available!";
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
		taskDlg.SetButtons(commandButtons.data(), (UINT)commandButtons.size());
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
		if(request_exit == EXIT_BY_MESSAGE)
			sta->EnableShowBoxOnClick(enable, [label, msg = (param == nullptr ? WM_CLOSE : (UINT)(DWORD_PTR)param)]() { ::SendMessage(::GetAncestor(VersionMessageLabel_GetHandle(label), GA_ROOTOWNER), msg, 0, 0); });
		else if(request_exit == EXIT_BY_DESTROYWINDOW)
			sta->EnableShowBoxOnClick(enable, [label]() { ::DestroyWindow(::GetAncestor(VersionMessageLabel_GetHandle(label), GA_ROOTOWNER));});
		else if (request_exit == EXIT_BY_ENDDIALOG)
			sta->EnableShowBoxOnClick(enable, [label, param]() { ::EndDialog(::GetAncestor(VersionMessageLabel_GetHandle(label), GA_ROOTOWNER), (INT_PTR)param); });
		else
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

void __stdcall VersionMessageLabel_SetAlignment(VersionMessageLabel label, Align align, Align lineAlign)
{
	auto sta = (UpdateStaticText*)label;
	sta->SetAlignment(align, lineAlign);
}

void __stdcall VersionMessageLabel_SetColor(VersionMessageLabel label, COLORREF color)
{
	auto sta = (UpdateStaticText*)label;
	sta->SetTextColor(color);
}

void __stdcall VersionMessageLabel_SetFont(VersionMessageLabel label, int nPointSize, LPCWSTR lpszFaceName)
{
	auto sta = (UpdateStaticText*)label;
	sta->SetFont(nPointSize, lpszFaceName);
}

void __stdcall VersionMessageLabel_SetAnchor(VersionMessageLabel label, UINT anchor, int left, int top, int right, int bottom)
{
	auto sta = (UpdateStaticText*)label;
	sta->SetAnchor(anchor, left, top, right, bottom);
}

VersionMessageWin __stdcall VersionMessageWin_Create(HWND parent, bool manage_update_instance)
{
	auto res = ExportHelper::GetInstance().CreateTextWin(parent);
	res->EnableManageUpdateInstance(manage_update_instance);
	if (manage_update_instance)
	{
		Update_Initialize();
		Update_StartWatch();
	}
	return (VersionMessageWin)res;
}
void __stdcall VersionMessageWin_SetShowingHandler(VersionMessageWin win, ShowingLabelEvent func, void* param)
{
	auto w = (UpdateTextWin*)win;
	if (func)
	{
		if (func == LABEL_TEXT_ALLCASE)
		{
			w->SetShowingHandler([w]() {
				auto msg = w->GetLatestMessage();
				return GetAllCaseText(msg);
			});
		}
		else
		{
			w->SetShowingHandler([w, func, param]() {
				std::wstring ws(MAX_LABLE_LEN, L'\0');
				auto msg = w->GetLatestMessage();
				func(msg, param, ws.data());
				return ws;
			});
		}
	}
	else
	{
		w->SetShowingHandler({});
	}
}
void __stdcall VersionMessageWin_SetColor(VersionMessageWin win, COLORREF bkColor, COLORREF textColor)
{
	auto w = (UpdateTextWin*)win;
	w->SetTextColor(bkColor, textColor);
}
void __stdcall VersionMessageWin_SetFont(VersionMessageWin win, int nPointSize, LPCTSTR lpszFaceName)
{
	auto w = (UpdateTextWin*)win;
	w->SetFont(nPointSize, lpszFaceName);
}
void __stdcall VersionMessageWin_SetAnchor(VersionMessageWin win, UINT anchor, int left, int top, int right, int bottom)
{
	auto w = (UpdateTextWin*)win;
	w->SetAnchor(anchor, left, top, right, bottom);
}
void __stdcall VersionMessageWin_EnableShowBoxOnClick(VersionMessageWin win, bool enable, UnargEvent request_exit, void* param)
{
	auto w = (UpdateTextWin*)win;
	if (request_exit)
	{
		if (request_exit == EXIT_BY_MESSAGE)
			w->EnableShowBoxOnClick(enable, [w, msg = (param == nullptr ? WM_CLOSE : (UINT)(DWORD_PTR)param)]() { ::SendMessage(::GetAncestor(w->GetParent(), GA_ROOTOWNER), msg, 0, 0); });
		else if (request_exit == EXIT_BY_DESTROYWINDOW)
			w->EnableShowBoxOnClick(enable, [w]() { ::DestroyWindow(::GetAncestor(w->GetParent(), GA_ROOTOWNER)); });
		else if (request_exit == EXIT_BY_ENDDIALOG)
			w->EnableShowBoxOnClick(enable, [w, param]() { ::EndDialog(::GetAncestor(w->GetParent(), GA_ROOTOWNER), (INT_PTR)param); });
		else
			w->EnableShowBoxOnClick(enable, [request_exit, param]() {request_exit(param); });
	}
	else
	{
		w->EnableShowBoxOnClick(enable, {});
	}
}
void __stdcall VersionMessageWin_EnablePerformUpdateOnExit(VersionMessageWin win, bool enable)
{
	auto w = (UpdateTextWin*)win;
	w->EnablePerformUpdateOnExit(enable);
}
VersionMessage __stdcall VersionMessageWin_GetVersionMessageRef(VersionMessageWin win)
{
	auto w = (UpdateTextWin*)win;
	return w->GetLatestMessage();
}
void __stdcall VersionMessageWin_SetTransparent(VersionMessageWin win, bool trans)
{
	auto w = (UpdateTextWin*)win;
	w->SetTransparent(trans);
}