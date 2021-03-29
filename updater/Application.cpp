#include "stdafx.h"
#include "Application.h"
#include "Reviser.h"
#include "WaitDialog.h"
#include "StringAlgo.h"
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <ryml/ryml.hpp>
#include <ryml/ryml_std.hpp>
#include <psapi.h>

namespace progopt = boost::program_options;

auto make_handle_ptr(HANDLE h)
{
	return std::unique_ptr<
		std::remove_pointer_t<HANDLE>,
		decltype(&::CloseHandle)>(h, &::CloseHandle);
}

void Application::init()
{
	global_settings.init();
}

void Application::out(const std::wstring& s) const
{
	std::cout << to_string(s) << std::endl;
}
void Application::err(const std::wstring& s) const
{
	std::cerr << to_string(s) << std::endl;
}
void Application::err(int exitCode, const std::wstring& s) const
{
	err(s);
	if (show_dialog)
	{
		MessageBoxW(nullptr, s.c_str(), L"Error", MB_ICONERROR);
	}
	std::exit(exitCode);
}
int Application::run()
{
	std::wstring cmdline = GetCommandLineW();
	std::wstring after_cmdline;
	if (auto pos = cmdline.find(L"--after "); pos != std::wstring::npos)
	{
		after_cmdline = cmdline.substr(pos + 8);
		cmdline.erase(pos);
	}

	auto args = progopt::split_winmain(cmdline);

	progopt::options_description desc("Update tool, Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("config,c", progopt::wvalue<std::wstring>(), "config file name")
		("output,o", progopt::wvalue<std::wstring>(), "output updater exefile name")
		("fetch,f", "Download objects and refs from another repository")
		("get-files,g", "Get files status")
		("get-log,l", "Get version status")
		("do-update,u", "Perform update, don't remove untracked files")
		("do-reset,r", "Perform update, remove untracked files")
		("before,b", progopt::wvalue<std::wstring>(), "run a command before update")
		("after,a", progopt::wvalue<std::wstring>(), "run a command after update")
		("wait,w", progopt::wvalue<int>(), "process id, wait it exit (5s).")
		("gui", "Show GUI dialog")
		("no-console", "Do not show console window")
		("restart", "restart software after updated, work with --wait argument");

	progopt::variables_map vm;
	progopt::store(progopt::wcommand_line_parser{ args }.options(desc).run(), vm);

	show_dialog = vm.find("gui") != vm.end();

	if (vm.find("no-console") != vm.end())
	{
		HWND hwndCon = GetConsoleWindow();
		::ShowWindow(hwndCon, SW_HIDE);
	}

	std::wstring restart_request;
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
		auto cmd = itr->second.as<std::wstring>();
		runCmd(std::move(cmd));
	}

	if (vm.empty() || vm.find("help") != vm.end()) {
		auto x = boost::lexical_cast<std::string>(desc);
		this->out(to_wstring(x));
		return 0;
	}

	if (auto itr = vm.find("config"); itr != vm.end())
	{
		loadSettings(itr->second.as<std::wstring>());
	}
	else
	{
		if (!loadSettingsFromSelf())
		{
			loadSettings(L"update.cfg");
		}
	}

	if (!verifySetting())
	{
		this->err(-1, L"Can not find config data!");
	}

	if (auto itr = vm.find("output"); itr != vm.end())
	{
		auto outfile = itr->second.as<std::wstring>();
		doOutput(outfile);
		return 0;
	}

	Reviser reviser;
	reviser.setRemoteBranch(to_string(global_settings.getBranch()));
	reviser.setRemoteSiteURL(to_string(global_settings.getRemoteUrl()));
	reviser.setRemoteSiteName("ccs");
	reviser.setWorkDir(to_string(global_settings.getLocalDir()));
	reviser.open();
	reviser.addIgnore("update~/");

	if (vm.find("fetch") != vm.end()) {
		if (!doFetch(reviser)) return -1;
	}
	
	if (vm.find("get-log") != vm.end()) {
		showStatus(reviser);
	}
	else if (vm.find("get-files") != vm.end()) {
		showFiles(reviser);
	}
	else if (vm.find("do-update") != vm.end()) {
		if (!doUpdate(reviser, false)) return -1;
	}
	else if (vm.find("do-reset") != vm.end()) {
		if (!doUpdate(reviser, true)) return -1;
	}
	
	if (after_cmdline.length() > 0)
	{
		runCmd(std::move(after_cmdline));
	}
	else if (auto itr = vm.find("after"); itr != vm.end())
	{
		auto cmd = itr->second.as<std::wstring>();
		runCmd(std::move(cmd));
	}

	if (!restart_request.empty())
	{
		runCmd(std::move(restart_request));
	}

	return 0;
}

bool Application::waitProcess(int proc_id, std::wstring* file_name)
{
	if (auto handle = make_handle_ptr(::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc_id)); handle)
	{
		if (file_name)
		{
			DWORD size = MAX_PATH;
			file_name->resize(size);
			if (QueryFullProcessImageName(handle.get(), 0, file_name->data(), &size))
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
				this->err(-1, L"the application is not closed");
				return false;
			}
		}
	}
	else
	{
		this->out(L"no process id, skip");
	}
	return true;
}

void Application::runCmd(std::wstring cmd)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	CreateProcessW(NULL, cmd.data(), NULL, NULL,
		FALSE, 0, NULL, NULL, &si, &pi
		);
	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

bool Application::doFetch(Reviser& reviser)
{
	if (show_dialog)
		return doFetchGui(reviser);
	else
		return doFetchCl(reviser);
}

bool Application::doFetchCl(Reviser& reviser)
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
	return true;
}

auto makeFetchGuiCallback(WaitDialog::WaitArgsWithMutex& args_w_m)
{
	return [&args_w_m](const TransferProgress& prog) {
		int pos = std::min<unsigned>(99, ::MulDiv(99, prog.received_objects, std::max<unsigned>(1, prog.total_objects)));		
		std::wstring buf(64, L'\0'); wsprintfW(buf.data(), L"Downloading...%d%%", pos);

		std::scoped_lock sl{ args_w_m.mutex };
		args_w_m.pos = pos;
		args_w_m.instructionText = L"Downloading files from remote server...";
		args_w_m.contantText = std::move(buf);
		return !args_w_m.cancelled;
	};
}

bool Application::doFetchGui(Reviser& reviser) noexcept
{
	WaitDialog::WaitArgsWithMutex args_w_m;
	args_w_m.title = L"Update";
	args_w_m.contantText = L"Downloading...";
	args_w_m.instructionText = L"Connecting to remote server...";
	args_w_m.enableCancelButton = false;

	try {
		WaitDialog::ShowAsync(
			[&reviser](auto& args) {reviser.fetch(makeFetchGuiCallback(args)); return true; },
			args_w_m
		);
		return true;
	}
	catch (...) {
		return false;
	}
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

template<class FUNC1, class FUNC2>
void putToNode(FUNC1&& get_msg, FUNC2&& put_to_node) noexcept
{
	try {
		auto res = std::forward<decltype(get_msg)>(get_msg)();
		std::forward<decltype(put_to_node)>(put_to_node)(std::move(res));
	}
	catch(...) {
		return;
	}
}

void Application::showStatus(Reviser& reviser) const
{
	ryml::Tree tree;
	auto r = tree.rootref();
	r |= ryml::MAP;

	auto statusNode = r["Status"];
	statusNode |= ryml::MAP;

	putToNode(
		[&reviser]() {return reviser.getWorkDirVersionMessage(); },
		[&statusNode](const std::string& msg) { auto node = statusNode["Local"]; putLines(node, msg); }
	);

	putToNode(
		[&reviser]() {return reviser.getRemoteVersionMessage(); },
		[&statusNode](const std::string& msg) { auto node = statusNode["Remote"]; putLines(node, msg); }
	);

	putToNode(
		[&reviser]() {return reviser.getDifferentVersionMessage(); },
		[&statusNode](const MessageList& msgs) {
			auto node = statusNode["New"];
			node |= ryml::SEQ;
			for (const auto& msg : msgs)
			{
				auto nodeItem = node.append_child();
				putLines(nodeItem, msg);
			}
		}
	);

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

bool Application::doUpdate(Reviser& reviser, bool reset)
{
	if (show_dialog)
	{
		WaitDialog::WaitArgsWithMutex args_w_m;
		args_w_m.title = L"Update";
		args_w_m.instructionText = L"Updating...";
		args_w_m.contantText = L"Please Wait...";
		args_w_m.enableCancelButton = false;

		try {
			WaitDialog::ShowAsync(
				[&reviser, reset](auto&) { reviser.sync(reset); return true; },
				args_w_m);
			return true;
		}
		catch (...) {
			return false;
		}
	}
	else
	{
		reviser.sync(reset);
	}
	return true;
}

bool Application::loadSettings(const std::wstring& fn)
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

void Application::doOutput(const std::wstring& outfile)
{
	if (!global_settings.createSelfExe(outfile))
	{
		err(-1, L"Create updater failed!");
	}
	else
	{
		out(L"Created!");
	}
}

