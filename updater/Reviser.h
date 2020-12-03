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

	void sync(bool remove_untrack = true); // ��Զ��һ��
	void fetch(FetchCallbackFunction callback); // ȡ��Զ����Ϣ
	std::string getWorkDirVersionMessage(); // �õ����ذ汾��Ϣ
	std::string getRemoteVersionMessage();  // �õ�Զ�̰汾��Ϣ
	MessageList getDifferentVersionMessage();  // �õ�Զ���뱾��֮��İ汾������Ϣ
	FileComparationList getDifferentFiles(); // �õ�Զ���뱾��֮���б仯���ļ�����ʽ��[delta]:[file]��

private:
	class Impl;
	Impl* impl;
};