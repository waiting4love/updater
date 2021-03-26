#include "pch.h"
#include "LatestInstance.h"
#include <algorithm>
LatestInstance::LatestInstance()
{
    ::GetModuleFileName(NULL, nameOfMutex, MAX_PATH);
    std::replace(std::begin(nameOfMutex), std::end(nameOfMutex), _T('\\'), _T('/'));
}

LatestInstance::~LatestInstance()
{
    Leave();
}

bool LatestInstance::Release()
{
    if (mutexInstance.m_h == nullptr) return false;
    mutexInstance.Close();
    return true;
}

bool LatestInstance::Acquire()
{
    if (mutexInstance.m_h == nullptr)
    {
        mutexInstance.Attach(CreateMutex(NULL, FALSE, nameOfMutex));
    }
    return mutexInstance.m_h != nullptr;
}

bool LatestInstance::Exists()
{
    CHandle h{ OpenMutex(SYNCHRONIZE, FALSE, nameOfMutex) };
    return h.m_h != nullptr;
}

bool LatestInstance::Enter(const volatile std::atomic_bool& Terminated)
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

bool LatestInstance::Leave()
{
    if (mutexInstance.m_h != nullptr && bLocked)
    {
        bLocked = false;
        return ::ReleaseMutex(mutexInstance);
    }
    return false;
}