#include "pch.h"
#include "LatestInstance.h"
#include <algorithm>
LatestInstance::LatestInstance()
{
    ::GetModuleFileName(NULL, nameOfMutex, MAX_PATH);
    std::replace(std::begin(nameOfMutex), std::end(nameOfMutex), _T('\\'), _T('/'));
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
