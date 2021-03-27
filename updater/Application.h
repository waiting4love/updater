#pragma once
#include "GlobalSettings.h"
class Reviser;
class Application
{
private:
	GlobalSettings global_settings;
	bool show_dialog = false;

	bool loadSettings(const std::wstring& fn);
	bool loadSettingsFromSelf();
	bool verifySetting() const;

	void doOutput(const std::wstring& outfile);
public:
	void init();
	void out(const std::wstring& s) const;
	void err(const std::wstring& s) const;
	void err(int exitCode, const std::wstring& s) const;
	int run();

	void runCmd(const std::wstring&);
	bool doFetch(Reviser&);
	bool doFetchGui(Reviser&) noexcept;
	bool doFetchCl(Reviser&);

	void showStatus(Reviser&) const;
	void showFiles(Reviser&);

	bool doUpdate(Reviser&, bool reset);

	bool waitProcess(int proc_id, std::wstring* file_name);
};

