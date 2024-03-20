#include "pch.h"
#include "LockerForUpdate.h"
#include <algorithm>

LockerForUpdate::LockerForUpdate()
{
    TCHAR baseName[MAX_PATH];
    ::GetModuleFileName(NULL, baseName, MAX_PATH);
    std::replace(std::begin(baseName), std::end(baseName), _T('\\'), _T('/'));
    lstrcpy(nameOfMutex, baseName);
    lstrcat(nameOfMutex, _T("/InstMu"));
}

LockerForUpdate::~LockerForUpdate()
{
    Leave();
}

bool LockerForUpdate::Release()
{
    if (mutexInstance.m_h == nullptr) return false;
    mutexInstance.Close();
    return true;
}

bool LockerForUpdate::Acquire()
{
    if (mutexInstance.m_h == nullptr)
    {
        mutexInstance.Attach(CreateMutex(NULL, FALSE, nameOfMutex));
    }
    return mutexInstance.m_h != nullptr;
}

bool LockerForUpdate::Exists()
{
    CHandle h{ OpenMutex(SYNCHRONIZE, FALSE, nameOfMutex) };
    return h.m_h != nullptr;
}

bool LockerForUpdate::Enter(const volatile std::atomic_bool& Terminated)
{
    const int span = 100;
    if (mutexInstance.m_h != nullptr)
    {
        while (!Terminated)
        {
            auto res = ::WaitForSingleObject(mutexInstance, span);
            if (res == WAIT_OBJECT_0)
            {
                bLocked = true;
                return true;
            }
            if (res == WAIT_TIMEOUT) continue;
            break;
        }
    }
    return false;
}

bool LockerForUpdate::Leave()
{
    if (mutexInstance.m_h != nullptr && bLocked)
    {
        bLocked = false;
        return ::ReleaseMutex(mutexInstance);
    }
    return false;
}
