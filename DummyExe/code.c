#define UNICODE
#define _UNICODE
#define WIN32

#include <tchar.h>
#include <windows.h>
#include <shlwapi.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(linker, "/align:512")
#pragma comment(linker, "/merge:.data=.text")
#pragma comment(linker, "/merge:.rdata=.text")
#pragma comment(linker, "/subsystem:windows")
#pragma comment(linker, "/ENTRY:main")

void main()
{
  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  TCHAR file[MAX_PATH];
  GetModuleFileName(NULL, file, MAX_PATH);

  TCHAR update_file[MAX_PATH];
  TCHAR update_param[MAX_PATH];
  lstrcpy(update_file, file);
  PathRemoveFileSpec(update_file);
  lstrcat(update_file, _T("\\update~\\updater.exe"));

  wsprintf(update_param,
      _T("-frw %d --gui --after \"%s\""),
      GetCurrentProcessId(), file);
  ShellExecute(NULL, _T("open"), update_file, update_param, NULL, SW_SHOW);
  CoUninitialize();
  ExitProcess(0);
}
