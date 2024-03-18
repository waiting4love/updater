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
//#pragma comment(lib, "Ole32.lib")
// #pragma comment(linker, "/align:512")
// #pragma comment(linker, "/merge:.data=.text")
// #pragma comment(linker, "/merge:.rdata=.text")
#pragma comment(linker, "/subsystem:windows")
#pragma comment(linker, "/ENTRY:myentry")

void myentry()
{
  TCHAR file[MAX_PATH];
  TCHAR update_file[MAX_PATH*2];
  TCHAR update_param[MAX_PATH*2];

  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  DWORD pid = GetCurrentProcessId();

  RtlSecureZeroMemory( &si, sizeof(si) );
  si.cb = sizeof(si);
  RtlSecureZeroMemory( &pi, sizeof(pi) );

  GetModuleFileName(NULL, file, MAX_PATH);
  lstrcpyn(update_file, file, MAX_PATH);
  PathRemoveFileSpec(update_file);
  PathAppend(update_file, _T("update~\\updater.exe"));
  wsprintf(update_param,
      _T("\"%s\" -frw %d --no-console --gui --after \"%s\""),
      update_file, pid, file);

  CreateProcess( NULL,   // No module name (use command line)
      update_param,        // Command line
      NULL,           // Process handle not inheritable
      NULL,           // Thread handle not inheritable
      FALSE,          // Set handle inheritance to FALSE
      0,              // No creation flags
      NULL,           // Use parent's environment block
      NULL,           // Use parent's starting directory 
      &si,            // Pointer to STARTUPINFO structure
      &pi );
  ExitProcess(0);
}
