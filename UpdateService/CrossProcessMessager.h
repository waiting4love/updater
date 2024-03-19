#pragma once

class CrossProcessMessager
{
private:
	UINT messageOfRequireExit;
	CHandle mutexMem;
	CHandle fileMapping;

	bool setRemind(bool val = true);
	void resetRemind();	
public:
	void RequireReboot(); // 发broadcast，请求关闭，最后一个关闭的负责重启
	bool TryRemind(); // 将remind标志置1，如果由0变1则返回true

	bool RebootRequired();
	UINT GetRequireRebootMessage() const;
	~CrossProcessMessager() = default;
	CrossProcessMessager();
};

