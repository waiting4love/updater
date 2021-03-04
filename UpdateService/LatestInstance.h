#pragma once
class LatestInstance
{
private:
	CHandle mutexInstance;
	TCHAR nameOfMutex[MAX_PATH];
public:
	LatestInstance();

	bool Release();
	bool Acquire();
	bool Exists();
};

