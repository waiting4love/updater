#include "stdafx.h"
#include "GlobalSettings.h"
#include "StringAlgo.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ryml.hpp>
#include <ryml_std.hpp>

namespace fs = std::filesystem;

constexpr const char* FLAG = "git\b";

struct TailFlag
{
	char flag[4];
	uint32_t size;
};

constexpr int FLAG_SIZE = sizeof(TailFlag);

void GlobalSettings::init()
{
	WCHAR buff[MAX_PATH] = { 0 };
	::GetModuleFileNameW(NULL, buff, MAX_PATH);
	fs::path path{ buff };
	exe_name = path.filename().wstring();
	exe_path = path.parent_path().wstring();
	exe_fullname = path.wstring();
}

void GlobalSettings::loadFromTree(void* ptree)
{
	remote_url = L"";
	branch = L"";
	local_dir = L"..";

	ryml::Tree& tree = *(ryml::Tree*)ptree;
	if (tree.size() > 2)
	{		
		if (auto root = tree.rootref(); root.valid() && root.has_child("git"))
		{
			auto git_node = root["git"];

			std::string u8_remote_url = "";
			std::string u8_branch = "";
			std::string u8_local_dir = "..";

			git_node.get_if<std::string>("remote", &u8_remote_url, "");
			git_node.get_if<std::string>("branch", &u8_branch, "");
			git_node.get_if<std::string>("local", &u8_local_dir, "..");

			remote_url = to_wstring(u8_remote_url);
			branch = to_wstring(u8_branch);
			local_dir = to_wstring(u8_local_dir);

			fs::path pathLocalDir = local_dir;
			if (pathLocalDir.is_relative())
			{
				fs::path path{ base_dir };
				path /= pathLocalDir;
				pathLocalDir = fs::weakly_canonical(path);
				local_dir = pathLocalDir.wstring();
			}
		}
	}
}

std::string GlobalSettings::saveToString() const
{
	ryml::Tree tree;
	auto r = tree.rootref();
	r |= ryml::MAP;
	auto node_git = r["git"];
	node_git |= ryml::MAP;

	node_git["remote"] << to_string(remote_url);
	node_git["branch"] << to_string(branch);
	node_git["local"] << to_string(local_dir);

	char buff[2048] = { 0 };	
	auto res = ryml::emit(tree, buff);
	return { res.begin(), res.end() };
}

bool GlobalSettings::loadFromFile(const std::wstring& file)
{
	fs::path pathFile{ file };
	if (pathFile.is_relative())
	{
		fs::path p{ exe_path };
		p /= pathFile;
		pathFile = fs::weakly_canonical(p);
	}

	base_dir = pathFile.parent_path().wstring();

	try {
		std::ifstream ifs{ pathFile };
		std::string str(std::istreambuf_iterator<char>{ifs}, {});
		ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(str));
		loadFromTree(&tree);
	}
	catch (...) {
		return false;
	}

	return !remote_url.empty();
}

bool GlobalSettings::loadFromSelf()
{
	base_dir = exe_path;

	std::ifstream ifs{ exe_fullname, std::ios::binary };
	if (ifs)
	{
		TailFlag flag = { 0 };

		ifs.seekg(-FLAG_SIZE, std::iostream::end).read((char*)&flag, sizeof(flag));
		if (memcmp(flag.flag, FLAG, sizeof(flag.flag)) == 0)
		{
			ifs.seekg(-FLAG_SIZE - (std::streamoff)flag.size, std::iostream::end);
			std::string s;
			s.resize(flag.size);
			ifs.read(s.data(), flag.size);

			try {
				ryml::Tree tree = ryml::parse_in_place(c4::to_substr(s));
				loadFromTree(&tree);
			}
			catch (...) {
				return false;
			}
			return !remote_url.empty();
		}
	}

	return false;
}

bool GlobalSettings::createSelfExe(const std::wstring& outfile) const
{
	std::ofstream ofs{ outfile, std::ios::binary };
	if (!ofs) return false;

	std::ifstream ifs{ getExeFullName(), std::ios::binary };
	if (!ifs) return false;

	TailFlag flag = { 0 };
	ifs.seekg(-FLAG_SIZE, std::iostream::end).read((char*)&flag, sizeof(flag));

	std::streamsize size = 1024 * 1024 * 16;  // max 16M

	if (memcmp(flag.flag, FLAG, sizeof(flag.flag)) == 0)
	{
		ifs.seekg(-FLAG_SIZE - (std::streamoff)flag.size, std::iostream::end);
		size = ifs.tellg();
	}

	char buff[1024];
	ifs.seekg(0, std::iostream::beg);
	while (ifs && ofs && size > 0)
	{
		auto len = std::min<std::streamsize>(sizeof(buff), size);
		ifs.read(buff, len);
		len = ifs.gcount();
		if (len == 0) break;
		ofs.write(buff, len);
		size -= len;
	}

	// write config, the 
	auto str = saveToString();
	ofs.write(str.c_str(), str.length());

	memcpy(flag.flag, FLAG, sizeof(flag.flag));
	flag.size = (decltype(flag.size))str.length();
	ofs.write((const char*)&flag, sizeof(flag));
	return true;
}

const std::wstring& GlobalSettings::getExeName() const
{
	return exe_name;
}

const std::wstring& GlobalSettings::getExeFullName() const
{
	return exe_fullname;
}

const std::wstring& GlobalSettings::getExePath() const
{
	return exe_path;
}

const std::wstring& GlobalSettings::getRemoteUrl() const
{
	return remote_url;
}

const std::wstring& GlobalSettings::getBranch() const
{
	return branch;
}

const std::wstring& GlobalSettings::getLocalDir() const
{
	return local_dir;
}

const std::wstring& GlobalSettings::getBaseDir() const
{
	return base_dir;
}
