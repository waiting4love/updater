#pragma once
#include "GlobalSettings.h"
class Reviser;
class Application
{
private:
	GlobalSettings global_settings;
	bool show_dialog = false;

	bool loadSettings(const std::string& fn);
	bool loadSettingsFromSelf();
	bool verifySetting() const;

	void doOutput(const std::string& outfile);
public:
	void init();
	void out(const std::string& s) const;
	void err(const std::string& s) const;
	void err(int exitCode, const std::string& s) const;
	int run();

	void runCmd(const std::string&);
	bool doFetch(Reviser&);
	bool doFetchGui(Reviser&);
	bool doFetchCl(Reviser&);

	void showStatus(Reviser&);
	void showFiles(Reviser&);

	bool doUpdate(Reviser&, bool reset);

	bool waitProcess(int proc_id, std::string* file_name);
};

