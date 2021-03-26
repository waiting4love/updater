#include "stdafx.h"
#include "GlobalSettings.h"
#include "StringAlgo.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ryml/ryml.hpp>
#include <ryml/ryml_std.hpp>

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
	exe_name = to_string(path.filename().wstring());
	exe_path = to_string(path.parent_path().wstring());
	exe_fullname = to_string(path.wstring());
}

void GlobalSettings::loadFromTree(void* ptree)
{
	remote_url = "";
	branch = "";
	local_dir = "..";

	ryml::Tree& tree = *(ryml::Tree*)ptree;
	if (tree.size() > 2)
	{		
		if (auto root = tree.rootref(); root.valid() && root.has_child("git"))
		{
			auto git_node = root["git"];
			git_node.get_if<std::string>("remote", &remote_url, "");
			git_node.get_if<std::string>("branch", &branch, "");
			git_node.get_if<std::string>("local", &local_dir, "..");

			fs::path pathLocalDir = local_dir;
			if (pathLocalDir.is_relative())
			{
				fs::path path{ to_wstring(base_dir) };
				path /= pathLocalDir;
				pathLocalDir = fs::weakly_canonical(path);
				local_dir = to_string(pathLocalDir.wstring());
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

	node_git["remote"] << remote_url;
	node_git["branch"] << branch;
	node_git["local"] << local_dir;

	char buff[2048] = { 0 };	
	auto res = ryml::emit(tree, buff);
	return { res.begin(), res.end() };
}

bool GlobalSettings::loadFromFile(const std::string& file)
{
	fs::path pathFile{ to_wstring(file) };
	if (pathFile.is_relative())
	{
		fs::path p{ to_wstring(exe_path) };
		p /= pathFile;
		pathFile = fs::weakly_canonical(p);
	}

	base_dir = to_string(pathFile.parent_path().wstring());

	try {
		std::ifstream ifs{ pathFile };
		std::string str(std::istreambuf_iterator<char>{ifs}, {});
		ryml::Tree tree = ryml::parse(c4::to_csubstr(str));
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
				ryml::Tree tree = ryml::parse(c4::to_substr(s));
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

bool GlobalSettings::createSelfExe(const std::string& outfile)
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
	flag.size = str.length();
	ofs.write((const char*)&flag, sizeof(flag));
	return true;
}

const std::string& GlobalSettings::getExeName() const
{
	return exe_name;
}

const std::string& GlobalSettings::getExeFullName() const
{
	return exe_fullname;
}

const std::string& GlobalSettings::getExePath() const
{
	return exe_path;
}

const std::string& GlobalSettings::getRemoteUrl() const
{
	return remote_url;
}

const std::string& GlobalSettings::getBranch() const
{
	return branch;
}

const std::string& GlobalSettings::getLocalDir() const
{
	return local_dir;
}

const std::string& GlobalSettings::getBaseDir() const
{
	return base_dir;
}
