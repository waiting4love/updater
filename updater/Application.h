#pragma once
#include "GlobalSettings.h"
class Reviser;
class Application
{
private:
	GlobalSettings global_settings;

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
	void doFetch(Reviser&);
	void showStatus(Reviser&);
	void showFiles(Reviser&);
	void doUpdate(Reviser&);
	void doReset(Reviser&);
};

