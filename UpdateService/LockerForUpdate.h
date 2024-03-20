#pragma once
#include <atomic>

class LockerForUpdate
{
private:
	CHandle mutexInstance;
	TCHAR nameOfMutex[MAX_PATH];
	bool bLocked{ false };
public:
	LockerForUpdate();
	~LockerForUpdate();

	bool Release();
	bool Acquire();
	bool Exists();
	bool Enter(const volatile std::atomic_bool& Terminated);
	bool Leave();

};

