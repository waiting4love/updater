#pragma once

DECLARE_HANDLE(VersionMessage);
DECLARE_HANDLE(VersionMessageLabel);
DECLARE_HANDLE(VersionMessageWin);

void __stdcall Update_Initialize();
void __stdcall Update_Uninitialize();
bool __stdcall Update_IsAvailable();
void __stdcall Update_SetGuiFetch(bool guiFetch);
using UpdateReceivedEvent = void(__stdcall*)(void* param);
bool __stdcall Update_StartWatch();
bool __stdcall Update_SetWatcher(UpdateReceivedEvent watcher, void* param);
bool __stdcall Update_StopWatch();
bool __stdcall Update_SetCheckInterval(int intervalMs);
bool __stdcall Update_Wait(int timeoutMs);
bool __stdcall Update_Perform(bool restart, const wchar_t* extra_args = nullptr);
bool __stdcall Update_IsError();
bool __stdcall Update_IsNewVersionReady();
bool __stdcall Update_IsNothing();
bool __stdcall Update_EnsureUpdateOnAppEntry(int timeoutMs = 30000, bool showProgressBar = true); // true if request restart
VersionMessage __stdcall Update_GetVersionMessage();
VersionMessage __stdcall Update_MoveVersionMessage();

bool __stdcall VersionMessage_Destory(VersionMessage);
bool __stdcall VersionMessage_IsError(VersionMessage);
bool __stdcall VersionMessage_IsNewVersionReady(VersionMessage);
bool __stdcall VersionMessage_IsNothing(VersionMessage);
int __stdcall VersionMessage_GetRemoteMessage(VersionMessage, wchar_t* message, int len);
int __stdcall VersionMessage_GetLocalMessage(VersionMessage, wchar_t* message, int len);
int __stdcall VersionMessage_GetWhatsNewMessage(VersionMessage, wchar_t* message, int len);
int __stdcall VersionMessage_GetErrorMessage(VersionMessage, wchar_t* message, int len);
int __stdcall VersionMessage_ShowBox(VersionMessage, HWND, const wchar_t* prompt, const wchar_t* buttons[], int buttons_size);

VersionMessageLabel __stdcall VersionMessageLabel_Create(HWND parent, LPRECT rect, UINT id = 0, bool manage_update_instance = false);
void __stdcall VersionMessageLabel_Destory(VersionMessageLabel); // it is not necessary
HWND __stdcall VersionMessageLabel_GetHandle(VersionMessageLabel);
VersionMessage __stdcall VersionMessageLabel_GetVersionMessageRef(VersionMessageLabel);
const size_t MAX_LABLE_LEN = 256;
using ShowingLabelEvent = void (__stdcall*)(VersionMessage, void* param, wchar_t* text); // max len = 256
#define LABEL_TEXT_DEFALUT nullptr
#define LABEL_TEXT_ALLCASE (ShowingLabelEvent)-1
void __stdcall VersionMessageLabel_SetShowingHandler(VersionMessageLabel, ShowingLabelEvent, void* param);
// request_exit is the action on user clicks update button, it should exit the software within 10 seconds.
// it can be a function or special number, see below 
using UnargEvent = void(__stdcall*)(void* param);
#define EXIT_BY_MESSAGE (UnargEvent)-1 // param = 0 will send WM_CLOSE, otherwise param is message id
#define EXIT_BY_DESTROYWINDOW (UnargEvent)-2
#define EXIT_BY_ENDDIALOG (UnargEvent)-3
void __stdcall VersionMessageLabel_EnableShowBoxOnClick(VersionMessageLabel label, bool enable, UnargEvent request_exit, void* param);
void __stdcall VersionMessageLabel_EnablePerformUpdateOnExit(VersionMessageLabel label, bool enable);
void __stdcall VersionMessageLabel_EnableAutoSize(VersionMessageLabel label, bool enable);
enum class Align: int { Near = 0, Far, Center };
void __stdcall VersionMessageLabel_SetAlignment(VersionMessageLabel label, Align align, Align lineAlign);
void __stdcall VersionMessageLabel_SetColor(VersionMessageLabel label, COLORREF color);
void __stdcall VersionMessageLabel_SetFont(VersionMessageLabel label, int nPointSize, LPCWSTR lpszFaceName);
const UINT ANCHOR_NONE = 0b0000;
const UINT ANCHOR_LEFT = 0b0001;
const UINT ANCHOR_TOP = 0b0010;
const UINT ANCHOR_RIGHT = 0b0100;
const UINT ANCHOR_BOTTOM = 0b1000;
void __stdcall VersionMessageLabel_SetAnchor(VersionMessageLabel label, UINT anchor, int left, int top, int right, int bottom);

VersionMessageWin __stdcall VersionMessageWin_Create(HWND parent, bool manage_update_instance = false);
void __stdcall VersionMessageWin_SetShowingHandler(VersionMessageWin win, ShowingLabelEvent func, void* param);
void __stdcall VersionMessageWin_SetColor(VersionMessageWin win, COLORREF bkColor, COLORREF textColor);
void __stdcall VersionMessageWin_SetFont(VersionMessageWin win, int nPointSize, LPCTSTR lpszFaceName);
void __stdcall VersionMessageWin_SetAnchor(VersionMessageWin win, UINT anchor, int left, int top, int right, int bottom);
void __stdcall VersionMessageWin_EnableShowBoxOnClick(VersionMessageWin win, bool enable, UnargEvent request_exit, void* param);
void __stdcall VersionMessageWin_EnablePerformUpdateOnExit(VersionMessageWin win, bool enable);
VersionMessage __stdcall VersionMessageWin_GetVersionMessageRef(VersionMessageWin win);
void __stdcall VersionMessageWin_SetTransparent(VersionMessageWin win, bool);
