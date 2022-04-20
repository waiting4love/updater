#pragma once

struct TransferProgress {
	unsigned int total_objects;
	unsigned int indexed_objects;
	unsigned int received_objects;
	unsigned int local_objects;
	unsigned int total_deltas;
	unsigned int indexed_deltas;
	size_t received_bytes;
};

enum class FileComparationDelta : int {
	UNMODIFIED,
	MODIFIED,
	DELETED,
	ADDED,
	RENAMED,
	COPIED,
	IGNORED,
	UNTRACKED,
	TYPECHANGE,
	UNREADABLE,
	CONFLICTED
};

struct FileComparationItem {
	FileComparationDelta delta;
	std::string file;
};
using FileComparationList = std::vector<FileComparationItem>;
using MessageList = std::vector<std::string>;
using FetchCallbackFunction = std::function<bool(const TransferProgress&)>;

struct ReviserException : std::exception
{
public:
	int getErrorCode() const;
	static int getLastErrorCode();
	static const char* getLastErrorMessage();
	static void throwError();
private:
	ReviserException(int errorCode, const char* message);
	int errorCode;
};

class Reviser
{
public:
	Reviser();
	~Reviser();

	void setWorkDir(std::string s);
	void setRemoteSiteName(std::string s);
	void setRemoteSiteURL(std::string s);
	void setRemoteBranch(std::string s);
	void addIgnore(const std::string& s);

	void open();
	void close();

	void sync(bool remove_untrack = true); // 与远程一致
	void fetch(FetchCallbackFunction callback); // 取得远程信息
	std::string getWorkDirVersionMessage(); // 得到本地版本信息
	std::string getRemoteVersionMessage();  // 得到远程版本信息
	MessageList getDifferentVersionMessage();  // 得到远程与本地之间的版本差异信息
	FileComparationList getDifferentFiles(); // 得到远程与本地之间有变化的文件（格式：[delta]:[file]）

private:
	class Impl;
	Impl* impl;
};