#pragma once

using UpdateWatcher = void(__stdcall*)(void* param);
DECLARE_HANDLE(VersionMessage);

bool __stdcall Update_Initialize();
bool __stdcall Update_Uninitialize();
bool __stdcall Update_IsAvailable();
bool __stdcall Update_StartWatch(int checkIntervalMs, UpdateWatcher watcher, void* param);
bool __stdcall Update_StopWatch();
bool __stdcall Update_Wait(int timeoutMs);
bool __stdcall Update_Perform(bool restart);
bool __stdcall Update_IsError();
bool __stdcall Update_IsNewVersionReady();
bool __stdcall Update_IsNothing();
VersionMessage __stdcall Update_GetVersionMessage();
bool __stdcall VersionMessage_Destory(VersionMessage);
bool __stdcall VersionMessage_IsError(VersionMessage);
bool __stdcall VersionMessage_IsNewVersionReady(VersionMessage);
bool __stdcall VersionMessage_IsNothing(VersionMessage);
int __stdcall VersionMessage_GetRemoteMessage(VersionMessage, char* message, int len);
int __stdcall VersionMessage_GetLocalMessage(VersionMessage, char* message, int len);
int __stdcall VersionMessage_GetWhatsNewMessage(VersionMessage, char* message, int len);
int __stdcall VersionMessage_GetErrorMessage(VersionMessage, char* message, int len);