#pragma once
#include <vector>
#include <string>
#include <functional>

using String = std::wstring;
using StringList = std::vector<String>;

struct VersionDetail
{
	StringList Local;
	StringList Remote;
	std::vector<StringList> New;
	bool isEmpty() const;
};

struct VersionInformation
{
	VersionDetail Status;
	String ErrorMessage;

	bool isNewVersionReady() const;
	bool isError() const;
	bool isEmpty() const;
	void setError(String message);
	static VersionInformation createError(String message);
	bool parse(const std::string& s);
};

class UpdateService
{
public:
	using VersionReceivedHandler = std::function<void()>;

	UpdateService();
	~UpdateService();
	UpdateService(const UpdateService&) = delete;
	UpdateService& operator=(const UpdateService&) = delete;

	void setUpdateExe(String exe_file);
	const String& getUpdateExe() const;
	void setCheckInterval(int ms);
	int getCheckInterval() const;
	void setRestartAppFlag(bool, const wchar_t* extra_args);
	void start();
	void stop();
	void setVersionReceivedHandler(VersionReceivedHandler);
	void removeVersionReceivedHandler();
	bool isAvailable() const;
	bool doUpdate() const;
	VersionInformation getVersionInfo();
	VersionInformation moveVersionInfo();
	bool waitVersionInfo(int timeout);
	bool isNewVersionReady() const;
	bool isError() const;
	bool IsNothing() const;
	void setGuiFetch(bool enableGui);
private:
	class Impl;
	Impl* _Impl;
};

