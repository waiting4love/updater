#pragma once

DECLARE_HANDLE(VersionMessage);
DECLARE_HANDLE(VersionMessageLabel);

void __stdcall Update_Initialize();
void __stdcall Update_Uninitialize();
bool __stdcall Update_IsAvailable();
using UpdateReceivedEvent = void(__stdcall*)(void* param);
bool __stdcall Update_StartWatch(int checkIntervalMs, UpdateReceivedEvent watcher, void* param);
bool __stdcall Update_StopWatch();
bool __stdcall Update_Wait(int timeoutMs);
bool __stdcall Update_Perform(bool restart);
bool __stdcall Update_IsError();
bool __stdcall Update_IsNewVersionReady();
bool __stdcall Update_IsNothing();
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
using ShowingLabelEvent = void (__stdcall*)(VersionMessageLabel, void* param, wchar_t* text); // max len = 256
using UnargEvent = void(__stdcall*)(void* param);
void __stdcall VersionMessageLabel_SetShowingLabelEvent(VersionMessageLabel, ShowingLabelEvent, void* param);
void __stdcall VersionMessageLabel_EnableShowBoxOnClick(VersionMessageLabel label, bool enable, UnargEvent request_exit, void* param);
void __stdcall VersionMessageLabel_EnablePerformUpdateOnExit(VersionMessageLabel label, bool enable);
void __stdcall VersionMessageLabel_EnableAutoSize(VersionMessageLabel label, bool enable);
void __stdcall VersionMessageLabel_SetAlignment(VersionMessageLabel label, bool RightAlign, bool BottomAlign);
void __stdcall VersionMessageLabel_SetColor(VersionMessageLabel label, COLORREF color);
void __stdcall VersionMessageLabel_SetFont(VersionMessageLabel label, int nPointSize, LPCWSTR lpszFaceName);
