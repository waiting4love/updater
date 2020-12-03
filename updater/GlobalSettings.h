#pragma once
class GlobalSettings
{
private:
	std::string exe_name;
	std::string exe_path;
	std::string exe_fullname;
	std::string base_dir;
	std::string remote_url;
	std::string branch;
	std::string local_dir;

	void loadFromTree(void*);
	std::string saveToString() const;
public:
	GlobalSettings();
	void init();
	bool loadFromFile(const std::string& file);
	bool loadFromSelf();
	bool createSelfExe(const std::string& file);

	const std::string& getExeName() const;
	const std::string& getExeFullName() const;
	const std::string& getExePath() const;
	const std::string& getRemoteUrl() const;
	const std::string& getBranch() const;
	const std::string& getLocalDir() const;
	const std::string& getBaseDir() const;
};

