#include "pch.h"
#include "UpdateService.h"
#include "LatestInstance.h"
#include <filesystem>
#include <tuple>
#include <sstream>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <codecvt>
#include "StringAlgo.h"
#include <shellapi.h>
#include "YamlDom.h"

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

class ExternalError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class UpdateService::Impl
{
private:
    WCHAR ExeFile[MAX_PATH];
    WCHAR ExtraArgs[MAX_PATH];
    DWORD IntervalMs = 60*1000;
    bool FlagForRestart = false;
    VersionReceivedHandler VersionReceived;
    bool Disabled = false;
    std::thread CheckThread;
    std::atomic_bool Terminated{ false };
    VersionInformation VersionInfo;
    mutable std::mutex VersionInfoMutex;       
    std::condition_variable VersionInfoReady;
    bool GuiFetch = false;
public:
    Impl()
    {
        ZeroMemory(ExeFile, sizeof(ExeFile));
        ZeroMemory(ExtraArgs, sizeof(ExtraArgs));
    }
    ~Impl()
    {
        stop();
    }
    void setUpdateExe(LPCTSTR file) { lstrcpyn(ExeFile, file, MAX_PATH); }
    LPCTSTR getUpdateExe() const { return ExeFile; }
    void setCheckInterval(int ms) { IntervalMs = ms; }
    int getCheckInterval() const { return IntervalMs; }
    void setRestartAppFlag(bool value, const wchar_t* extra_args) {
        FlagForRestart = value;
        if (extra_args != nullptr)
            lstrcpynW(ExtraArgs, extra_args, MAX_PATH);
        else
            ZeroMemory(ExtraArgs, sizeof(ExtraArgs));
    }
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
    void removeVersionReceivedHandler() { VersionReceived = VersionReceivedHandler{}; }
    
    bool isAvailable() const
    {
        return !Disabled && fs::exists(ExeFile);
    }
    void disable() { Disabled = true; }
    void enable() { Disabled = false; }
    void setGuiFetch(bool enable) { GuiFetch = enable; }
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
            TCHAR Args[MAX_PATH*2];

            wsprintf(Args, _T("--no-console --gui -rw %d"), pid);
            if (FlagForRestart)
            {
                _tcscat_s(Args, _T(" --after "));
                if (ExtraArgs[0] != 0)
                {
                    _tcscat_s(Args, ExtraArgs);
                }
                else
                {
                    _tcscat_s(Args, _T("\""));
                    _tcscat_s(Args, ThisFile);
                    _tcscat_s(Args, _T("\""));
                }
            }
            ::ShellExecute(NULL, _T("open"), NewExe, Args, NULL, SW_SHOW);
            return true;
        }

        return false;
    }
    VersionInformation getVersionInfo()
    {
        VersionInformation res;
        std::scoped_lock<std::mutex> lg(VersionInfoMutex);
        res = VersionInfo;
        return res;
    }
    VersionInformation moveVersionInfo()
    {
        VersionInformation res;
        std::scoped_lock<std::mutex> lg(VersionInfoMutex);
        std::swap(res, VersionInfo);
        return res;
    }
    bool waitVersionInfo(int timeout)
    {
        std::unique_lock<std::mutex> lk(VersionInfoMutex);
        if (!VersionInfo.isEmpty()) return true;
        return VersionInfoReady.wait_for(lk, std::chrono::milliseconds(timeout), [this]() { return !VersionInfo.isEmpty(); });
    }
    bool isNewVersionReady() const
    {
        std::scoped_lock<std::mutex> lg(VersionInfoMutex);
        return VersionInfo.isNewVersionReady();
    }
    bool isError() const
    {
        std::scoped_lock<std::mutex> lg(VersionInfoMutex);
        return VersionInfo.isError();
    }
    bool IsNothing() const
    {
        std::scoped_lock<std::mutex> lg(VersionInfoMutex);
        return VersionInfo.isEmpty();
    }
private:
    // 产生支持异步的管道
	static BOOL APIENTRY CreatePipeEx(
            OUT LPHANDLE lpReadPipe,
            OUT LPHANDLE lpWritePipe,
            IN LPSECURITY_ATTRIBUTES lpPipeAttributes,
            IN DWORD nSize,
            DWORD dwReadMode,
            DWORD dwWriteMode
        )
        /*++
        Routine Description:
            The CreatePipeEx API is used to create an anonymous pipe I/O device.
            Unlike CreatePipe FILE_FLAG_OVERLAPPED may be specified for one or
            both handles.
            Two handles to the device are created.  One handle is opened for
            reading and the other is opened for writing.  These handles may be
            used in subsequent calls to ReadFile and WriteFile to transmit data
            through the pipe.
        Arguments:
            lpReadPipe - Returns a handle to the read side of the pipe.  Data
                may be read from the pipe by specifying this handle value in a
                subsequent call to ReadFile.
            lpWritePipe - Returns a handle to the write side of the pipe.  Data
                may be written to the pipe by specifying this handle value in a
                subsequent call to WriteFile.
            lpPipeAttributes - An optional parameter that may be used to specify
                the attributes of the new pipe.  If the parameter is not
                specified, then the pipe is created without a security
                descriptor, and the resulting handles are not inherited on
                process creation.  Otherwise, the optional security attributes
                are used on the pipe, and the inherit handles flag effects both
                pipe handles.
            nSize - Supplies the requested buffer size for the pipe.  This is
                only a suggestion and is used by the operating system to
                calculate an appropriate buffering mechanism.  A value of zero
                indicates that the system is to choose the default buffering
                scheme.
        Return Value:
            TRUE - The operation was successful.
            FALSE/NULL - The operation failed. Extended error status is available
                using GetLastError.
        --*/

    {
        static volatile long PipeSerialNumber = 1;

        HANDLE ReadPipeHandle;
        HANDLE WritePipeHandle;
        DWORD dwError;
        TCHAR PipeNameBuffer[MAX_PATH];

        //
        // Only one valid OpenMode flag - FILE_FLAG_OVERLAPPED
        //

        if ((dwReadMode | dwWriteMode) & (~FILE_FLAG_OVERLAPPED)) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        //
        //  Set the default timeout to 120 seconds
        //

        if (nSize == 0) {
            nSize = 4096;
        }

        wsprintf(PipeNameBuffer,
            _T("\\\\.\\Pipe\\RemoteExeAnon.%08x.%08x"),
            GetCurrentProcessId(),
            InterlockedIncrement(&PipeSerialNumber)
        );

        ReadPipeHandle = CreateNamedPipe(
            PipeNameBuffer,
            PIPE_ACCESS_INBOUND | dwReadMode,
            PIPE_TYPE_BYTE | PIPE_WAIT,
            1,             // Number of pipes
            nSize,         // Out buffer size
            nSize,         // In buffer size
            120 * 1000,    // Timeout in ms
            lpPipeAttributes
        );

        if (!ReadPipeHandle) {
            return FALSE;
        }

        WritePipeHandle = CreateFile(
            PipeNameBuffer,
            GENERIC_WRITE,
            0,                         // No sharing
            lpPipeAttributes,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | dwWriteMode,
            NULL                       // Template file
        );

        if (INVALID_HANDLE_VALUE == WritePipeHandle) {
            dwError = GetLastError();
            CloseHandle(ReadPipeHandle);
            SetLastError(dwError);
            return FALSE;
        }

        *lpReadPipe = ReadPipeHandle;
        *lpWritePipe = WritePipeHandle;
        return(TRUE);
    }


    static std::tuple<int, std::string> executeCommand(LPCTSTR Command, LPCTSTR Args, const volatile std::atomic_bool& Terminated, bool showWindow = false)
    {
        CHandle hRead;
        CHandle hWrite;

        SECURITY_ATTRIBUTES sa = { 0 };
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        if(!CreatePipeEx(&hRead.m_h, &hWrite.m_h, &sa, 0, FILE_FLAG_OVERLAPPED, FILE_FLAG_OVERLAPPED))
        {
            WindowsError::throwLastError("CreatePipe");
        }

        STARTUPINFO si = { 0 };
        PROCESS_INFORMATION pi = { 0 };
        si.cb = sizeof(STARTUPINFO);
        si.hStdInput = INVALID_HANDLE_VALUE;
        si.hStdError = hWrite;
        si.hStdOutput = hWrite;
        si.wShowWindow = showWindow?SW_SHOW:SW_HIDE;
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
                Args2[0] = ' ';
            _tcscat_s(Args2, Args);
            res = CreateProcess(Command, Args2, NULL, NULL, TRUE, NULL, NULL, NULL, &si, &pi);
        }

        if (res == FALSE)
            WindowsError::throwLastError("CreateProcess");
        
        // If they are not explicitly closed, there is no way to recognize that the child process has ended.
        hWrite.Close();
        const DWORD span = 100;

        CHandle hEvent{ ::CreateEvent(NULL, TRUE, TRUE, NULL) };
        OVERLAPPED oOverlap = { 0 };
        oOverlap.hEvent = hEvent;

        std::stringstream ss;
        while (!Terminated)
        {
            DWORD bytesRead;
            char c;
            res = ReadFile(hRead, &c, 1, &bytesRead, &oOverlap);
            if (res == FALSE && GetLastError() == ERROR_IO_PENDING)
            {
                while (!Terminated && ::WaitForSingleObject(hEvent, span) == WAIT_TIMEOUT);
                if (!Terminated)
                {
					res = GetOverlappedResult(
						hRead, // handle to pipe 
						&oOverlap, // OVERLAPPED structure 
						&bytesRead,            // bytes transferred 
						FALSE);
                }
            }

            if (res == TRUE)
            {
                if (bytesRead == 0) break;
                ss.write(&c, 1);
            }
            else
            {
                break;
            }
        }

        while (!Terminated && ::WaitForSingleObject(pi.hProcess, span) == WAIT_TIMEOUT);

        std::tuple<int, std::string> result;
        if (!Terminated)
        {
            DWORD exitCode = 0;
            ::GetExitCodeProcess(pi.hProcess, &exitCode);
            result = std::make_tuple(exitCode, ss.str());
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
    void fetch() const
    {
        if (!isAvailable()) return;
        LatestInstance li;
        li.Acquire();
        li.Enter(Terminated);
        auto [code, output] = executeCommand(
            getUpdateExe(),
            (GuiFetch? _T("-f --gui --no-console") :_T("-f")),
            Terminated,
            (GuiFetch ? true : false));
        
        if (code != 0)
        {
            throw ExternalError{ output };
        }
    }
    VersionInformation readVersionInfomation() const
    {
        LatestInstance li;
        li.Acquire();
        li.Enter(Terminated);
        auto [code, output] = executeCommand(getUpdateExe(), _T("-l"), Terminated);
        if (code == 0)
        {
            VersionInformation vi;
            if (!vi.parse(output))
            {
                throw ExternalError{ "Cannot parse the output!" };
            }
            return vi;
        }
        else
        {
            throw ExternalError{ output };
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
                {
                    std::scoped_lock<std::mutex> lg(VersionInfoMutex);
                    VersionInfo = std::move(vd);
                }
                VersionInfoReady.notify_one();
                if (VersionReceived) VersionReceived();

            }
            catch (const std::exception& ex)
            {
                auto vd = VersionInformation::createError(to_wstring(ex.what()));
                {
                    std::scoped_lock<std::mutex> lg(VersionInfoMutex);
                    VersionInfo = std::move(vd);
                }
                VersionInfoReady.notify_one();
                if (VersionReceived) VersionReceived();
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
        (Status.Local.empty() || !Status.New.empty() || Status.Remote != Status.Local);
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

bool VersionInformation::parse(const std::string& s)
{
    try {
        yaml::Sequence empty_seq;
        auto tree = yaml::parseString(s);
        auto& status = tree.getMapping().value().getMapping("Status").value();
        auto& local = status.getSequence("Local").value_or(empty_seq);
        auto& remote = status.getSequence("Remote").value_or(empty_seq);
        auto& whatnew = status.getSequence("New").value_or(empty_seq);

        Status.Local.clear();
        std::transform(local.begin(), local.end(), std::back_inserter(Status.Local), [](auto& i) {return to_wstring(i.getString().value()); });

        Status.Remote.clear();
        std::transform(remote.begin(), remote.end(), std::back_inserter(Status.Remote), [](auto& i) {return to_wstring(i.getString().value()); });

        Status.New.clear();
        std::transform(
            whatnew.begin(), whatnew.end(), std::back_inserter(Status.New),
            [](auto& i) {
                StringList lst;
                auto& seq = i.getSequence().value();
                std::transform(seq.begin(), seq.end(), std::back_inserter(lst), [](auto& i) {return to_wstring(i.getString().value()); });
                return lst;
            }
        );
        return true;
    }
    catch (...) {
        return false;
    }
}

bool VersionDetail::isEmpty() const
{
    return Local.empty() && Remote.empty() && New.empty();
}
///////////////////////////////////////////////////////
UpdateService::UpdateService()
{
    _Impl = new Impl;
    TCHAR buf[MAX_PATH];
    ::GetModuleFileName(NULL, buf, MAX_PATH);
    ::PathRemoveFileSpec(buf);
    ::PathAppend(buf, _T("update\\updater.exe"));
    _Impl->setUpdateExe(buf);
    _Impl->setCheckInterval(10 * 60 * 1000);
}

UpdateService::~UpdateService()
{
    delete _Impl;
}

void UpdateService::setUpdateExe(String exe_file)
{
    _Impl->setUpdateExe(exe_file.c_str());
}

const String& UpdateService::getUpdateExe() const
{   
    return _Impl->getUpdateExe();
}

void UpdateService::setCheckInterval(int ms)
{
    _Impl->setCheckInterval(ms);
}

int UpdateService::getCheckInterval() const
{
    return _Impl->getCheckInterval();
}

void UpdateService::setRestartAppFlag(bool r, const wchar_t* extra_args)
{
    _Impl->setRestartAppFlag(r, extra_args);
}

void UpdateService::start()
{
    _Impl->start();
}

void UpdateService::stop()
{
    _Impl->stop();
}

void UpdateService::setVersionReceivedHandler(VersionReceivedHandler func)
{
    _Impl->setVersionReceivedHandler(std::move(func));
}

void UpdateService::removeVersionReceivedHandler()
{
    _Impl->removeVersionReceivedHandler();
}

bool UpdateService::isAvailable() const
{
    return _Impl->isAvailable();
}

bool UpdateService::doUpdate() const
{
    return _Impl->doUpdate();
}

VersionInformation UpdateService::getVersionInfo()
{
    return _Impl->getVersionInfo();
}

VersionInformation UpdateService::moveVersionInfo()
{
    return _Impl->moveVersionInfo();
}

bool UpdateService::waitVersionInfo(int timeout)
{
    return _Impl->waitVersionInfo(timeout);
}

bool UpdateService::isNewVersionReady() const
{
    return _Impl->isNewVersionReady();
}

bool UpdateService::isError() const
{
    return _Impl->isError();
}

bool UpdateService::IsNothing() const
{
    return _Impl->IsNothing();
}

void UpdateService::setGuiFetch(bool enableGui)
{
    _Impl->setGuiFetch(enableGui);
}