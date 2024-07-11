#include "pch.h"
#include "CrossProcessMessager.h"
#include <algorithm>
#include <chrono>
using clock_type = std::chrono::system_clock;

struct CrossData
{
    bool hasRemind;
    clock_type::time_point clockOnRequireReboot;
};

template<class T>
class SharedMem
{
private:
    T* pData{ nullptr };
    HANDLE mutex;
    bool shouldReleaseMutex{ true };
public:
    SharedMem(HANDLE mapping, HANDLE mutex)
        :mutex(mutex)
    {
        if (!mutex || !mapping) return;
        shouldReleaseMutex = WaitForSingleObject(mutex, 300) == WAIT_OBJECT_0;
        pData = (T*)MapViewOfFile(
            mapping,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(T));
    }
    ~SharedMem()
    {
        if (pData)
        {
            UnmapViewOfFile(pData);
        }
        if (shouldReleaseMutex) ReleaseMutex(mutex);
    }
    operator bool() const
    {
        return (bool)pData;
    }

    T* operator->() const 
    {
        return pData;
    }
};

using SharedData = SharedMem<CrossData>;

CrossProcessMessager::CrossProcessMessager()
{
    TCHAR nameOfMem[MAX_PATH];
    TCHAR nameOfMutex[MAX_PATH];

    TCHAR baseName[MAX_PATH] = { 0 };
    ::GetModuleFileName(NULL, baseName, MAX_PATH);
    std::replace(std::begin(baseName), std::end(baseName), _T('\\'), _T('/'));
    messageOfRequireExit = RegisterWindowMessage(baseName);

    lstrcpy(nameOfMem, baseName);
    lstrcat(nameOfMem, _T("/Mem"));
    fileMapping.Attach(CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SharedData),
        nameOfMem));

    lstrcpy(nameOfMutex, baseName);
    lstrcat(nameOfMutex, _T("/MemMu"));
    mutexMem.Attach(CreateMutex(NULL, FALSE, nameOfMutex));

    if (ERROR_ALREADY_EXISTS != GetLastError()) {
        SharedData data{ fileMapping, mutexMem };
        data->clockOnRequireReboot = {};
        data->hasRemind = false;
    }
}

void CrossProcessMessager::RequireReboot()
{
    {
        SharedData data{ fileMapping, mutexMem };
        data->clockOnRequireReboot = clock_type::now();
    }

    SendNotifyMessage(HWND_BROADCAST, messageOfRequireExit, 0, 0);
}

bool CrossProcessMessager::RebootRequired()
{
    SharedData data{ fileMapping, mutexMem };
    return clock_type::now() - data->clockOnRequireReboot < std::chrono::seconds{ 10 };
}

bool CrossProcessMessager::TryRemind()
{
    return setRemind(true);
}

UINT CrossProcessMessager::GetRequireRebootMessage() const
{
    return messageOfRequireExit;
}

bool CrossProcessMessager::setRemind(bool val)
{
    if (!mutexMem) return false;

    bool changed = false;
    SharedData data{ fileMapping, mutexMem };
    if (data)
    {
        if (data->hasRemind != val)
        {
            data->hasRemind = val;
            changed = true;
        }
    }    
    return changed;
}
void CrossProcessMessager::resetRemind()
{
    setRemind(false);
}