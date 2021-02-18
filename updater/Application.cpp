#include "stdafx.h"
#include "Application.h"
#include "Reviser.h"
#include "WaitDialog.h"
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
		MessageBoxA(nullptr, s.c_str(), "Error", MB_ICONERROR);
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

	if (vm.empty() || vm.find("help") != vm.end()) {
		auto x = boost::lexical_cast<std::string>(desc);
		this->out(x);
		return 0;
	}

	if (auto itr = vm.find("config"); itr != vm.end())
	{
		loadSettings(itr->second.as<std::string>());
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
		{
			if (!doFetchGui(reviser)) return -1;
		}
		else
		{
			doFetch(reviser);
		}
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

std::wstring to_wstring(const std::string& s, UINT code_page = CP_ACP)
{
	std::wstring ws;
	ws.resize(::MultiByteToWideChar(code_page, 0, s.c_str(), s.length(), nullptr, 0));
	::MultiByteToWideChar(code_page, 0, s.c_str(), s.length(), ws.data(), ws.size());
	return ws;
}

bool Application::doFetchGui(Reviser& reviser)
{
	WaitDialog::WaitArgsWithMutex args_w_m;

	auto cb = [&args_w_m](const TransferProgress& prog) {
		int pos = std::min<unsigned>(99, ::MulDiv(99, prog.received_objects, std::max<unsigned>(1, prog.total_objects)));
		std::wstring buf(64, L'\0');
		wsprintfW(buf.data(), L"Downloading...%d%%", pos);
		std::scoped_lock sl{ args_w_m.mutex };
		args_w_m.args.pos = pos;
		args_w_m.args.contantText = std::move(buf);
		return !args_w_m.args.cancelled;
	};

	auto proc = [&reviser, &cb](WaitDialog::WaitArgsWithMutex& args) {
		std::wstring errStr;
		try {
			reviser.fetch(std::move(cb));
			args.args.pos = 999; //completed
		} catch (const std::exception& ex) {
			errStr = to_wstring(ex.what());
		} catch (...) {
			errStr = L"An unexpected error has occurred";
		}
		if (!errStr.empty()) {
			std::scoped_lock sl{ args.mutex };
			args.args.contantText = std::move(errStr);
			args.args.status = WaitDialog::Status::Error;
			args.args.enable_cancel_button = true;
			return false;
		}
		return true;
	};

	args_w_m.args.title = L"Update";
	args_w_m.args.instructionText = L"Connecting to remote server...";
	args_w_m.args.enable_cancel_button = false;
	return WaitDialog::WaitProgress<bool>(std::move(proc), args_w_m);
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

