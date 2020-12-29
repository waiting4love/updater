#include "stdafx.h"
#include "UpdateService.h"
#include <filesystem>
#include <tuple>
#include <sstream>
#include <thread>
#include <future>
#include <atomic>
#include <ryml/ryml.hpp>
#include <ryml/ryml_std.hpp>

namespace fs = std::filesystem;

struct WindowsError {
    DWORD code;
    std::wstring msg;
    const char* file;
    int line;
    const char* function;

    WindowsError(DWORD code, std::wstring msg = L"", const char* file = __builtin_FILE(), int line = __builtin_LINE(), const char* function = __builtin_FUNCTION()) :
        code(code),
        msg(std::move(msg)),
        file(file),
        line(line),
        function(function)
    {}

    std::wstring wwhat() const noexcept {
        std::wostringstream ss;
        ss << file << L':' << line << L" (" << function << L')';
        if (!msg.empty()) {
            ss << L" ← " << msg;
        }
        ss << ": " << code << L", " << wstrerror(code);
        return ss.str();
    }

    static std::wstring wstrerror(DWORD error)
    {
        std::unique_ptr<wchar_t, decltype(&LocalFree)> errmsg(nullptr, &LocalFree);
        {
            wchar_t* errmsg_p = nullptr;
            if (!FormatMessageW(
                FORMAT_MESSAGE_FROM_SYSTEM
                | FORMAT_MESSAGE_ALLOCATE_BUFFER
                | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<wchar_t*>(&errmsg_p),
                0, nullptr
            )) {
                std::wostringstream ss;
                ss << L"[FormatMessage failure: " << GetLastError() << ']';
                return ss.str();
            }
            errmsg.reset(errmsg_p);
        }
        return errmsg.get();
    }

    static void throwLastError(const char* msg)
    {
        throw std::system_error(GetLastError(), std::system_category(), msg);
    }
};

class UpdateService::Impl
{
private:
    TCHAR ExeFile[MAX_PATH];
    DWORD IntervalMs;
    bool FlagForRestart = false;
    VersionReceivedHandler VersionReceived;
    bool Disabled = false;
    std::thread CheckThread;
    std::atomic_bool Terminated{ false };
public:
    void setUpdateExe(LPCTSTR file) { lstrcpyn(ExeFile, file, MAX_PATH); }
    LPCTSTR getUpdateExe() const { return ExeFile; }
    void setCheckInterval(int ms) { IntervalMs = ms; }
    int getCheckInterval() const { return IntervalMs; }
    void setRestartAppFlag(bool value) { FlagForRestart = value; }
    void start()
    {
        if (!isAvailable() || CheckThread.joinable()) return;
        resetTerminate();
        CheckThread = std::thread([this]() {checkStatusProc(); });
    }
    void stop()
    {
        if (CheckThread.joinable())
        {
            setTerminate();
            CheckThread.join();
        }
    }
    void setVersionReceivedHandler(VersionReceivedHandler func) { VersionReceived = std::move(func); }
    bool isAvailable() const
    {
        return !Disabled && fs::exists(ExeFile);
    }
    void disable() { Disabled = true; }
    void enable() { Disabled = false; }
    bool doUpdate() const
    {
        TCHAR ThisFile[MAX_PATH];
        ::GetModuleFileName(NULL, ThisFile, MAX_PATH);
        TCHAR NewPath[MAX_PATH];
        ::lstrcpyn(NewPath, ThisFile, MAX_PATH);
        ::PathRemoveFileSpec(NewPath);
        ::PathAppend(NewPath, _T("update~"));
        TCHAR SrcPath[MAX_PATH];
        ::lstrcpyn(SrcPath, ExeFile, MAX_PATH);
        ::PathRemoveFileSpec(SrcPath);

        try
        {
            copyDirectory(SrcPath, NewPath, true);
            ::SetFileAttributes(NewPath, ::GetFileAttributes(NewPath) | FILE_ATTRIBUTE_HIDDEN);
        }
        catch(...)
        {
            //
        }
        TCHAR NewExe[MAX_PATH];
        ::PathCombine(NewExe, NewPath, ::PathFindFileName(ExeFile));

        if (fs::exists(NewExe))
        {
            DWORD pid = ::GetCurrentProcessId();
            TCHAR Args[MAX_PATH];

            wsprintf(Args, _T("--gui -rw %d"), pid);
            if (FlagForRestart)
            {
                _tcscat_s(Args, _T(" --after \""));
                _tcscat_s(Args, ThisFile);
                _tcscat_s(Args, _T("\""));
            }
            ::ShellExecute(NULL, _T("open"), NewExe, Args, NULL, SW_SHOW);
            return true;
        }

        return false;
    }
private:
    std::tuple<int, std::string> executeCommand(LPCTSTR Command, LPCTSTR Args)
    {
        CHandle hRead;
        CHandle hWrite;

        SECURITY_ATTRIBUTES sa = { 0 };
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        if (!::CreatePipe(&hRead.m_h, &hWrite.m_h, &sa, 0))
        {
            WindowsError::throwLastError("CreatePipe");
        }

        STARTUPINFO si = { 0 };
        PROCESS_INFORMATION pi = { 0 };
        si.cb = sizeof(STARTUPINFO);
        si.hStdInput = INVALID_HANDLE_VALUE;
        si.hStdError = hWrite;
        si.hStdOutput = hWrite;
        si.wShowWindow = SW_HIDE;
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

        BOOL res;
        if (Args == NULL || Args[0] == 0)
        {
            res = CreateProcess(Command, NULL, NULL, NULL, TRUE, NULL, NULL, NULL, &si, &pi);
        }
        else
        {
            TCHAR Args2[MAX_PATH * 2] = { 0 };
            if (Args[0] != ' ')
            {
                Args2[0] = ' ';
            }
            _tcscat_s(Args2, Args);
            res = CreateProcess(Command, Args2, NULL, NULL, TRUE, NULL, NULL, NULL, &si, &pi);
        }

        if (res == FALSE)
        {
            WindowsError::throwLastError("CreateProcess");
        }
        
        // create thread to receive console
        auto future_str = std::async(std::launch::async, [&hRead, this]() {
            std::stringstream ss;
            DWORD bytesRead;
            char buffer[128] = { 0 };
            while (!Terminated && ReadFile(hRead, buffer, 128, &bytesRead, NULL))
            {
                if (bytesRead == 0) break;
                ss.write(buffer, bytesRead);
            }
            return ss.str();
        });

        const DWORD span = 100;
        while (!Terminated && ::WaitForSingleObject(pi.hProcess, span) == WAIT_TIMEOUT );
        hWrite.Close();
        future_str.wait();

        std::tuple<int, std::string> result;
        if (!Terminated)
        {
            DWORD exitCode = 0;
            ::GetExitCodeProcess(pi.hProcess, &exitCode);
            result = std::make_tuple( exitCode, future_str.get());
        }
        else
        {
            result = std::make_tuple(-2, "Terminated!");
        }

        ::CloseHandle(pi.hThread);
        ::CloseHandle(pi.hProcess);
        return result;
    }
    static void copyDirectory(const fs::path& sourceDir, const fs::path& destDir, bool copySubDirs)
    {
        auto copyOptions = fs::copy_options::overwrite_existing;
        if (copySubDirs)
        {
            copyOptions |= fs::copy_options::recursive;
        }
        fs::copy(sourceDir, destDir, copyOptions);
    }
    void fetch()
    {
        if (!isAvailable()) return;
        auto [code, output] = executeCommand(getUpdateExe(), _T("--fetch"));
        if (code != 0)
        {
            throw std::exception(output.c_str());
        }
    }
    VersionInformation readVersionInfomation()
    {
        auto [code, output] = executeCommand(getUpdateExe(), _T("-l"));
        if (code == 0)
        {
            VersionInformation vi;
            if (!vi.parse(output))
            {
                throw std::exception("cannot parse the output!");
            }
            return vi;
        }
        else
        {
            throw std::exception(output.c_str());
        }
    }
    void resetTerminate()
    {
        Terminated = false;
    }
    void setTerminate() // 设置标志，在executeCommand中时直接throw terminate error, 在sleep时直接退出
    {
        Terminated = true;
    }
    void checkStatusProc()
    {
        while (!Terminated)
        {
            try
            {
                fetch();
                auto vd = readVersionInfomation();
                if (VersionReceived) VersionReceived(vd);
            }
            catch (const std::exception& ex)
            {
                auto vd = VersionInformation::createError(ex.what());
                if (VersionReceived) VersionReceived(vd);
            }

            const DWORD span = 100;
            for (DWORD i = 0; i < IntervalMs && !Terminated; i += span)
            {
                Sleep(span);
            }
        }
    }

};


bool VersionInformation::isNewVersionReady() const
{
    return
        ErrorMessage.empty() &&
        !Status.Remote.empty() &&
        (Status.Local.empty() || !Status.New.empty());
}

bool VersionInformation::isError() const
{
    return !ErrorMessage.empty();
}

bool VersionInformation::isEmpty() const
{
    return ErrorMessage.empty() && Status.isEmpty();
}

VersionInformation VersionInformation::createError(String message)
{
    VersionInformation res;
    res.ErrorMessage = std::move(message);
    return res;
}

StringList getStringLines(const ryml::NodeRef& node)
{
    StringList res;
    if (node.is_seq())
    {
        for (const auto &s : node)
        {
            res.push_back(s.val().str);
        }
    }
    return res;
}

bool VersionInformation::parse(const std::string& s)
{
    ryml::Tree tree = ryml::parse(c4::to_csubstr(s));
    if (tree.size() <= 2) return false;
    if (auto root = tree.rootref(); root.valid() && root.has_child("Status"))
    {
        auto status_node = root["Status"];
        if (status_node.has_child("Local"))
        {
            Status.Local = getStringLines(status_node["Local"]);
        }
        if (status_node.has_child("Remote"))
        {
            Status.Remote = getStringLines(status_node["Remote"]);
        }
        if (status_node.has_child("New"))
        {
            auto new_node = status_node["New"];
            if (new_node.is_seq())
            {
                for (const auto& x: new_node)
                {
                    Status.New.push_back(getStringLines( x ));
                }
            }
        }

        return true;
    }
    return false;
}

bool VersionDetail::isEmpty() const
{
    return Local.empty() && Remote.empty() && New.empty();
}
