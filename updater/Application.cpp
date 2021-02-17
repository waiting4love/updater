#include "stdafx.h"
#include "Application.h"
#include "Reviser.h"
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <ryml/ryml.hpp>
#include <ryml/ryml_std.hpp>
#include <psapi.h>
#include <CommCtrl.h>
#include <future>

namespace progopt = boost::program_options;

auto make_handle_ptr(HANDLE h)
{
	return std::unique_ptr<
		std::remove_pointer<HANDLE>::type,
		decltype(&::CloseHandle)>(h, &::CloseHandle);
}

void Application::init()
{
	global_settings.init();
}

void Application::out(const std::string& s) const
{
	std::cout << s << std::endl;
}
void Application::err(const std::string& s) const
{
	std::cerr << s << std::endl;
}
void Application::err(int exitCode, const std::string& s) const
{
	err(s);
	if (show_dialog)
	{
		MessageBoxA(NULL, s.c_str(), "Error", MB_ICONERROR);
	}
	std::exit(exitCode);
}
int Application::run()
{
	auto args = progopt::split_winmain(GetCommandLineA());

	progopt::options_description desc("Update tool, Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("config,c", progopt::value<std::string>(), "config file name")
		("output,o", progopt::value<std::string>(), "output updater exefile name")
		("fetch,f", "Download objects and refs from another repository")
		("get-files,g", "Get files status")
		("get-log,l", "Get version status")
		("do-update,u", "Perform update, don't remove untracked files")
		("do-reset,r", "Perform update, remove untracked files")
		("before,b", progopt::value<std::string>(), "run a command before update")
		("after,a", progopt::value<std::string>(), "run a command after update")
		("wait,w", progopt::value<int>(), "process id, wait it exit (5s).")
		("gui", "Show GUI dialog")
		("no-console", "Do not show console window")
		("restart", "restart software after updated, work with --wait argument");

	progopt::variables_map vm;
	progopt::store(progopt::command_line_parser{ args }.options(desc).run(), vm);

	show_dialog = vm.find("gui") != vm.end();

	if (vm.find("no-console") != vm.end())
	{
		HWND hwndCon = GetConsoleWindow();
		::ShowWindow(hwndCon, SW_HIDE);
	}

	std::string restart_request;
	if (auto itr = vm.find("wait"); itr != vm.end())
	{
		auto proc_id = itr->second.as<int>();
		if (vm.find("restart") != vm.end())
		{
			if(!waitProcess(proc_id, &restart_request)) return -1;
		}
		else
		{
			if(!waitProcess(proc_id, nullptr)) return -1;
		}
	}

	if (auto itr = vm.find("before"); itr != vm.end())
	{
		auto cmd = itr->second.as<std::string>();
		runCmd(cmd);
	}

	if (vm.size() == 0 || vm.find("help") != vm.end()) {
		auto x = boost::lexical_cast<std::string>(desc);
		this->out(x);
		return 0;
	}

	if (auto itr = vm.find("config"); itr != vm.end())
	{
		auto cfgfile = itr->second.as<std::string>();
		loadSettings(cfgfile);
	}
	else
	{
		if (!loadSettingsFromSelf())
		{
			loadSettings("update.cfg");
		}
	}

	if (!verifySetting())
	{
		this->err(-1, "Can not find config data!");
	}

	if (auto itr = vm.find("output"); itr != vm.end())
	{
		auto outfile = itr->second.as<std::string>();
		doOutput(outfile);
		return 0;
	}

	Reviser reviser;
	reviser.setRemoteBranch(global_settings.getBranch());
	reviser.setRemoteSiteURL(global_settings.getRemoteUrl());
	reviser.setRemoteSiteName("ccs");
	reviser.setWorkDir(global_settings.getLocalDir());
	reviser.open();
	reviser.addIgnore("update~/");

	if (vm.find("fetch") != vm.end()) {
		if (show_dialog)
			if (!doFetchGui(reviser)) return -1;
		else
			doFetch(reviser);
	}
	
	if (vm.find("get-log") != vm.end()) {
		showStatus(reviser);
	}
	else if (vm.find("get-files") != vm.end()) {
		showFiles(reviser);
	}
	else if (vm.find("do-update") != vm.end()) {
		doUpdate(reviser);
	}
	else if (vm.find("do-reset") != vm.end()) {
		doReset(reviser);
	}
	
	if (auto itr = vm.find("after"); itr != vm.end())
	{
		auto cmd = itr->second.as<std::string>();
		runCmd(cmd);
	}

	if (!restart_request.empty())
	{
		runCmd(restart_request);
	}

	return 0;
}

bool Application::waitProcess(int proc_id, std::string* file_name)
{
	if (auto handle = make_handle_ptr(::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc_id)); handle)
	{
		if (file_name)
		{
			DWORD size = MAX_PATH;
			file_name->resize(size);
			if (QueryFullProcessImageNameA(handle.get(), 0, file_name->data(), &size))
			{
				file_name->resize(size);
			}
			else
			{
				file_name->clear();
			}
		}

		while (::WaitForSingleObject(handle.get(), 10000) != WAIT_OBJECT_0)
		{
			if (show_dialog)
			{
				if (IDCANCEL == MessageBox(
					NULL,
					L"The application is not closed, please close application and try again",
					L"Update",
					MB_ICONWARNING | MB_RETRYCANCEL))
				{
					return false;
				}
			}
			else
			{
				this->err(-1, "the application is not closed");
				return false;
			}
		}
	}
	else
	{
		this->out("no process id, skip");
	}
	return true;
}

void Application::runCmd(const std::string& cmd)
{
	::ShellExecuteA(NULL, "open", cmd.c_str(), NULL, NULL, SW_SHOW);
}

void Application::doFetch(Reviser& reviser)
{
	auto cb = [this, pos=std::make_shared<int>(0)](const TransferProgress& prog) {
		int cur = 50 * prog.received_objects / std::max<unsigned>(1, prog.total_objects);
		if (int x = cur - *pos; x > 0)
		{
			std::cout << std::string( x, '.' );
			std::cout.flush();
			*pos = cur;
		}
		return true;
	};
	reviser.fetch(std::move(cb));
	std::cout << std::endl;
}

struct TaskDialogData
{
	std::atomic_int pos = 0;
	std::string str;
	int prev_pos = -1;
};

HRESULT CALLBACK TaskDialogCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData)
{
	TaskDialogData* data = (TaskDialogData*)lpRefData;
	int val = data->pos;
	int prev_pos = data->prev_pos;
	switch (uMsg)
	{
	case TDN_DIALOG_CONSTRUCTED:
		break;
	case TDN_CREATED:
		::SendMessage(hWnd, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(0, 100));
		::SendMessage(hWnd, TDM_ENABLE_BUTTON, IDCLOSE, FALSE);
		break;
	case TDN_BUTTON_CLICKED:
		break;
	case TDN_RADIO_BUTTON_CLICKED:
		break;
	case TDN_HYPERLINK_CLICKED:
		break;
	case TDN_EXPANDO_BUTTON_CLICKED:
		break;
	case TDN_VERIFICATION_CLICKED:
		break;
	case TDN_HELP:
		break;
	case TDN_TIMER:
		if (prev_pos != val)
		{
			if (val < 0)
			{
				std::string s = data->str;
				std::wstring ws;
				if (s.empty())
				{
					ws = L"An error occurs!";
				}
				else
				{
					ws.resize(::MultiByteToWideChar(CP_ACP, 0, s.c_str(), s.length(), NULL, 0));
					::MultiByteToWideChar(CP_ACP, 0, s.c_str(), s.length(), ws.data(), ws.size());
				}
				::SendMessage(hWnd, TDM_ENABLE_BUTTON, IDCLOSE, TRUE);
				::SendMessage(hWnd, TDM_SET_PROGRESS_BAR_STATE, PBST_ERROR, 0L);
				::SendMessage(hWnd, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)ws.c_str());
				::SendMessage(hWnd, TDM_UPDATE_ICON, TDIE_ICON_MAIN, (LPARAM)TD_ERROR_ICON);
			}
			else if (val > 100)
			{
				::EndDialog(hWnd, IDOK);
			}
			else if (val == 0)
			{
				::SendMessage(hWnd, TDM_SET_PROGRESS_BAR_MARQUEE, 1, 50);
			}
			else if (val > 0)
			{
				if (prev_pos <= 0)
				{
					::SendMessage(hWnd, TDM_SET_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, (LPARAM)L"Downloading data from remote server...");
					::SendMessage(hWnd, TDM_SET_MARQUEE_PROGRESS_BAR, 0, 0);
				}
				WCHAR buf[64];
				wsprintfW(buf, L"Downloading...%d%%", val);
				::SendMessage(hWnd, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)buf);
				::SendMessage(hWnd, TDM_SET_PROGRESS_BAR_POS, val, 0L);
			}
		}
		data->prev_pos = val;
		break;
	case TDN_NAVIGATED:
		break;
	case TDN_DESTROYED:
		break;
	default:
		break;
	}

	return S_OK;
}

bool Application::doFetchGui(Reviser& reviser)
{
	using PFN_TaskDialogIndirect = HRESULT(STDAPICALLTYPE*)(const TASKDIALOGCONFIG* pTaskConfig, int* pnButton, int* pnRadioButton, BOOL* pfVerificationFlagChecked);
	PFN_TaskDialogIndirect pfnTaskDialogIndirect = nullptr;

	std::unique_ptr<std::remove_pointer_t<HMODULE>, decltype(&FreeLibrary)> hCommCtrlDLL{ ::LoadLibrary(L"comctl32.dll"), &FreeLibrary };
	if (hCommCtrlDLL)
	{
		pfnTaskDialogIndirect = (PFN_TaskDialogIndirect)::GetProcAddress(hCommCtrlDLL.get(), "TaskDialogIndirect");
	}

	if (pfnTaskDialogIndirect == nullptr)
	{
		doFetch(reviser);
		return true;
	}

	TaskDialogData dialogData;
	std::atomic_bool exit = false;
	auto cb = [&dialogData, &exit](const TransferProgress& prog) {
		dialogData.pos = std::min<unsigned>(99, ::MulDiv(99, prog.received_objects, std::max<unsigned>(1, prog.total_objects)));
		return !exit;
	};

	auto fut = std::async(std::launch::async, [&reviser, &cb, &dialogData]() {
		try {
			reviser.fetch(std::move(cb));
			dialogData.pos = 999;
		}
		catch (const std::exception& ex) {
			dialogData.str = ex.what();
			dialogData.pos = -1;
		}
	});

	TASKDIALOGCONFIG tdc = { 0 };
	tdc.cbSize = sizeof(TASKDIALOGCONFIG);
	tdc.pszWindowTitle = L"Update";
	tdc.pszMainInstruction = L"Connecting to remote server...";
	tdc.dwFlags = TDF_SHOW_MARQUEE_PROGRESS_BAR | TDF_CALLBACK_TIMER;
	tdc.dwCommonButtons = TDCBF_CLOSE_BUTTON;
	tdc.lpCallbackData = (LONG_PTR)&dialogData;
	tdc.pfCallback = TaskDialogCallback;
	pfnTaskDialogIndirect(&tdc, nullptr, nullptr, nullptr);

	exit = true;
	fut.wait();
	return dialogData.pos >= 100;
}

void putLines(ryml::NodeRef& node, const std::string& lines)
{
	node |= ryml::SEQ;
	std::stringstream ss(lines);
	std::string line;
	while (std::getline(ss, line))
	{
		node.append_child() << line;
	}
}

void Application::showStatus(Reviser& reviser)
{
	ryml::Tree tree;
	auto r = tree.rootref();
	r |= ryml::MAP;

	auto statusNode = r["Status"];
	statusNode |= ryml::MAP;

	try {
		auto msg = reviser.getWorkDirVersionMessage();
		auto node = statusNode["Local"];
		putLines(node, msg);
	}
	catch (...)
	{
	}

	try {
		auto msg = reviser.getRemoteVersionMessage();
		auto node = statusNode["Remote"];
		putLines(node, msg);
	}
	catch (...)
	{
	}

	try {
		auto msgs = reviser.getDifferentVersionMessage();
		auto node = statusNode["New"];
		node |= ryml::SEQ;
		for (const auto& msg : msgs)
		{
			auto nodeItem = node.append_child();
			putLines(nodeItem, msg);
		}
	}
	catch (...)
	{
	}

	ryml::emit(tree);
}

void Application::showFiles(Reviser& reviser)
{
	ryml::Tree tree;
	auto r = tree.rootref();
	r |= ryml::MAP;
	auto node = r["Files"];
	node |= ryml::SEQ;

	auto files = reviser.getDifferentFiles();
	for (const auto& file : files)
	{
		auto item = node.append_child();
		item |= ryml::MAP;
		item["Delta"] << (int)file.delta;
		item["File"] << file.file;
	}

	ryml::emit(tree);
}

void Application::doUpdate(Reviser& reviser)
{
	reviser.sync(false);
}

void Application::doReset(Reviser& reviser)
{
	reviser.sync(true);
}

bool Application::loadSettings(const std::string& fn)
{
	return global_settings.loadFromFile(fn);
}

bool Application::loadSettingsFromSelf()
{
	return global_settings.loadFromSelf();
}

bool Application::verifySetting() const
{
	return !global_settings.getRemoteUrl().empty();
}

void Application::doOutput(const std::string& outfile)
{
	if (!global_settings.createSelfExe(outfile))
	{
		err(-1, "Create updater failed!");
	}
	else
	{
		out("Created!");
	}
}

