#include "stdafx.h"
#include "GlobalSettings.h"
#include <filesystem>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
namespace pt = boost::property_tree;

constexpr const char* FLAG = "git\b";

struct TailFlag
{
	char flag[4];
	uint32_t size;
};

constexpr int FLAG_SIZE = sizeof(TailFlag);

GlobalSettings::GlobalSettings()
{
}

void GlobalSettings::init()
{
	WCHAR buff[MAX_PATH] = { 0 };
	::GetModuleFileNameW(NULL, buff, MAX_PATH);
	fs::path path{ buff };
	exe_name = path.filename().string();
	exe_path = path.parent_path().string();
	exe_fullname = path.string();
}

void GlobalSettings::loadFromTree(void* ptree)
{
	pt::ptree& tree = *(pt::ptree*)ptree;
	remote_url = tree.get<std::string>("git.remote", "");
	branch = tree.get<std::string>("git.branch", "master");
	fs::path pathLocalDir = tree.get<std::string>("git.local", "..");
	if (pathLocalDir.is_relative())
	{
		fs::path path{ base_dir };
		path /= pathLocalDir;
		pathLocalDir = fs::weakly_canonical(path);
	}
	local_dir = pathLocalDir.string();
}

std::string GlobalSettings::saveToString() const
{
	pt::ptree tree;
	tree.put("git.remote", remote_url);
	tree.put("git.branch", branch);
	tree.put("git.local", local_dir);

	std::stringstream ss;
	pt::write_xml(ss, tree);
	return ss.str();
}

bool GlobalSettings::loadFromFile(const std::string& file)
{
	fs::path pathFile{ file };
	if (pathFile.is_relative())
	{
		fs::path p{ exe_path };
		p /= pathFile;
		pathFile = fs::weakly_canonical(p);
	}

	base_dir = pathFile.parent_path().string();

	pt::ptree tree;
	try {
		std::ifstream ifs{ pathFile };
		pt::read_xml(ifs, tree);
	}
	catch (std::exception&) {
		return false;
	}

	loadFromTree(&tree);

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

			std::stringstream ss{ s };
			pt::ptree tree;
			try {
				pt::read_xml(ss, tree);
			}
			catch (std::exception&) {
				return false;
			}
			loadFromTree(&tree);
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
