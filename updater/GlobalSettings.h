#pragma once
class GlobalSettings
{
private:
	std::wstring exe_name;
	std::wstring exe_path;
	std::wstring exe_fullname;
	std::wstring base_dir;
	std::wstring remote_url;
	std::wstring branch;
	std::wstring local_dir;

	void loadFromTree(void*);
	std::string saveToString() const;
public:
	GlobalSettings() = default;
	void init();
	bool loadFromFile(const std::wstring& file);
	bool loadFromSelf();
	bool createSelfExe(const std::wstring& file) const;

	const std::wstring& getExeName() const;
	const std::wstring& getExeFullName() const;
	const std::wstring& getExePath() const;
	const std::wstring& getRemoteUrl() const;
	const std::wstring& getBranch() const;
	const std::wstring& getLocalDir() const;
	const std::wstring& getBaseDir() const;
};

