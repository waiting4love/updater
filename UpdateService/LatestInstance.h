#pragma once
#include <atomic>

class LatestInstance
{
private:
	CHandle mutexInstance;
	TCHAR nameOfMutex[MAX_PATH];
	bool bLocked{ false };
public:
	LatestInstance();
	~LatestInstance();

	bool Release();
	bool Acquire();
	bool Exists();
	bool Enter(const volatile std::atomic_bool& Terminated);
	bool Leave();
};

