#include "stdafx.h"
#include "Reviser.h"
#include <git2.h>

typedef bool(__stdcall* FetchCallback)(TransferProgress*, void*);
typedef bool(__stdcall* MessageCallback)(const char*, void*);
typedef bool(__stdcall* DiffCallback)(const char*, FileComparationDelta, void*);

class Reviser::Impl
{
public:
	~Impl();

	void setWorkDir(std::string s);
	void setRemoteSiteName(std::string s);
	void setRemoteSiteURL(std::string s);
	void setRemoteBranch(std::string s);
	bool addIgnore(const std::string& s);

	bool open();
	bool close();

	// low level
	bool fetch(FetchCallback callback, void*); // 取得远程信息
	bool getWorkDirVersionMessage(char* buf, int len); // 得到本地版本信息
	bool getRemoteVersionMessage(char* buf, int len);  // 得到远程版本信息
	bool getDifferentVersionMessage(MessageCallback callback, void* param);  // 得到远程与本地之间的版本差异信息
	bool getDifferentFiles(DiffCallback callback, void* param); // 得到远程与本地之间有变化的文件（格式：[delta]:[file]）
	bool sync(bool remove_untrack = true); // 与远程一致

private:
	std::string work_dir; // 本地目录
	std::string remote_site_name; // 远程地址别名
	std::string remote_site_url; // 远程地址
	std::string remote_branch; // 远程分支

	git_repository* repo = nullptr;
	git_remote* remote = nullptr;
	bool initialized = false;
};

int ReviserException::getErrorCode() const
{
	return errorCode;
}

void ReviserException::throwError()
{
	throw ReviserException(getLastErrorCode(), getLastErrorMessage());
}

int ReviserException::getLastErrorCode()
{
	const git_error* e = giterr_last();
	return e->klass;
}

const char* ReviserException::getLastErrorMessage()
{
	const git_error* e = giterr_last();
	return e->message;
}

ReviserException::ReviserException(int errorCode, const char* message)
	:std::exception(message)
{
	this->errorCode = errorCode;
}

static void Check(bool res)
{
	if (!res) ReviserException::throwError();
}

Reviser::Impl::~Impl()
{
	close();
}

void Reviser::Impl::setWorkDir(std::string s)
{
	work_dir = std::move(s);
}

void Reviser::Impl::setRemoteSiteName(std::string s)
{
	remote_site_name = std::move(s);
}

void Reviser::Impl::setRemoteSiteURL(std::string s)
{
	remote_site_url = std::move(s);
}

void Reviser::Impl::setRemoteBranch(std::string s)
{
	remote_branch = std::move(s);
}

bool Reviser::Impl::addIgnore(const std::string& s)
{
	int error = git_ignore_add_rule(repo, s.c_str());
	return error >= 0;
}

// 打开或初始化repo，设置远程名称与路径。
// 找到或新建work_branch
// checkout到work_branch
bool Reviser::Impl::open()
{
	int error = git_libgit2_init();
	if (error < 0) return false;
	initialized = true;

	// 打开或初始化repo
	error = git_repository_open(&repo, work_dir.c_str());
	if (error == GIT_ENOTFOUND)
	{
		error = git_repository_init(&repo, work_dir.c_str(), false);
	}

	if (error < 0) return false;

	// 设置远程名称与路径
	error = git_remote_lookup(&remote, repo, remote_site_name.c_str());
	if (error == GIT_ENOTFOUND) {
		// 如果没有得到，建立
		error = git_remote_create(&remote, repo, remote_site_name.c_str(), remote_site_url.c_str());
	}
	if (error < 0) return false;

	return error >= 0;
}

bool Reviser::Impl::close()
{
	if (remote) git_remote_free(remote);
	if (repo) git_repository_free(repo);
	if (initialized) git_libgit2_shutdown();
	remote = nullptr;
	repo = nullptr;
	initialized = false;
	return true;
}

struct FetchPayload
{
	FetchCallback callback;
	void* param;
};

int fetch_cb(const git_transfer_progress* stats, void* payload)
{
	TransferProgress tmp;
	tmp.total_objects = stats->total_objects;
	tmp.indexed_objects = stats->indexed_objects;
	tmp.received_objects = stats->received_objects;
	tmp.local_objects = stats->local_objects;
	tmp.total_deltas = stats->total_deltas;
	tmp.indexed_deltas = stats->indexed_deltas;
	tmp.received_bytes = stats->received_bytes;

	auto fp = reinterpret_cast<FetchPayload*>(payload);
	auto res = fp->callback(&tmp, fp->param);
	return res ? 0 : -1;
}

bool Reviser::Impl::fetch(FetchCallback callback, void* param)
{
	FetchPayload fp;
	fp.callback = callback;
	fp.param = param;
	git_fetch_options opts = GIT_FETCH_OPTIONS_INIT;
	opts.callbacks.payload = std::addressof(fp);
	opts.callbacks.transfer_progress = fetch_cb;
	int error = git_remote_fetch(remote, NULL, &opts, NULL);
	return error >= 0;
}

bool Reviser::Impl::getWorkDirVersionMessage(char* buf, int len)
{
	git_commit* head_commit = NULL;
	int error = git_revparse_single((git_object**)&head_commit, repo, "HEAD");
	if (error < 0) return false;
	strncpy_s(buf, len, git_commit_message(head_commit), len);
	git_commit_free(head_commit);
	return error >= 0;
}

bool Reviser::Impl::getRemoteVersionMessage(char* buf, int len)
{
	git_commit* remote_header = NULL;
	std::string remote = remote_site_name + "/" + remote_branch;
	int error = git_revparse_single((git_object**)&remote_header, repo, remote.c_str());
	if (error < 0) return false;
	strncpy_s(buf, len, git_commit_message(remote_header), len);
	git_commit_free(remote_header);
	return error >= 0;
}

bool Reviser::Impl::getDifferentVersionMessage(MessageCallback callback, void* param)
{
	// git log HEAD..ccs/master --oneline 显示版本变化
	git_revwalk* walker;
	int error = git_revwalk_new(&walker, repo);
	if (error < 0) return false;
	std::string str_range = "HEAD.." + remote_site_name + "/" + remote_branch;
	error = git_revwalk_push_range(walker, str_range.c_str());
	if (error >= 0)
	{
		git_oid oid;
		bool run = true;
		while (run && !git_revwalk_next(&oid, walker)) {
			git_commit* c = NULL;
			error = git_commit_lookup(&c, repo, &oid);
			if (error < 0) break;
			run = callback(git_commit_message(c), param);
			git_commit_free(c);
		}
	}
	git_revwalk_free(walker);
	return error >= 0;
}

int each_file_cb(const git_diff_delta* delta, float progress, void* payload)
{
	auto payload_pair = (std::pair<DiffCallback, void*>*)payload;

	const char* file = delta->old_file.path;
	if (!file || file[0] == 0) {
		file = delta->new_file.path;
	}
	FileComparationDelta cd;

	switch (delta->status)
	{
	case GIT_DELTA_UNMODIFIED: cd = FileComparationDelta::UNMODIFIED; break;
	case GIT_DELTA_MODIFIED: cd = FileComparationDelta::MODIFIED; break;
	case GIT_DELTA_DELETED: cd = FileComparationDelta::DELETED; break;
	case GIT_DELTA_ADDED: cd = FileComparationDelta::ADDED; break;
	case GIT_DELTA_RENAMED: cd = FileComparationDelta::RENAMED; break;
	case GIT_DELTA_COPIED: cd = FileComparationDelta::COPIED; break;
	case GIT_DELTA_IGNORED: cd = FileComparationDelta::IGNORED; break;
	case GIT_DELTA_UNTRACKED: cd = FileComparationDelta::UNTRACKED; break;
	case GIT_DELTA_TYPECHANGE: cd = FileComparationDelta::TYPECHANGE; break;
	case GIT_DELTA_UNREADABLE: cd = FileComparationDelta::UNREADABLE; break;
	case GIT_DELTA_CONFLICTED: cd = FileComparationDelta::CONFLICTED; break;
	default: cd = FileComparationDelta::UNMODIFIED; break;
	}

	bool res = payload_pair->first(file, cd, payload_pair->second);
	return res ? 0 : 1; // 如果返回0，会继续调用后面更详细的cb，返回1，则结束
}

bool Reviser::Impl::getDifferentFiles(DiffCallback callback, void* param)
{
	// git diff --stat ccs/master  显示修改(与workdir对比)
	git_object* obj = NULL;
	git_tree* tree = NULL;
	git_diff* diff = NULL;
	auto payload = std::make_pair(callback, param);

	std::string remote_tree = remote_site_name + "/" + remote_branch + "^{tree}";
	int error = git_revparse_single(&obj, repo, remote_tree.c_str());
	if (error < 0) goto FINALEND;
	error = git_tree_lookup(&tree, repo, git_object_id(obj));
	if (error < 0) goto FREEOBJ;
	error = git_diff_tree_to_workdir(&diff, repo, tree, NULL);
	if (error < 0) goto FREETREE;

	error = git_diff_foreach(diff,
		each_file_cb,
		NULL,//each_binary_cb,
		NULL,//each_hunk_cb,
		NULL,//each_line_cb,
		&payload);
	// goto FREEDIFF

//FREEDIFF:
	git_diff_free(diff);
FREETREE:
	git_tree_free(tree);
FREEOBJ:
	git_object_free(obj);
FINALEND:
	return error >= 0;
}

bool Reviser::Impl::sync(bool remove_untrack)
{
	// reset --hard， 清除本地修改，直接与远程相同
	git_commit* remote_header = NULL;
	std::string remote_url = remote_site_name + "/" + remote_branch;
	int error = git_revparse_single((git_object**)&remote_header, repo, remote_url.c_str());
	if (error < 0) return false;
	error = git_reset(repo, (git_object*)remote_header, GIT_RESET_HARD, NULL);
	git_commit_free(remote_header);
	if (error < 0) return false;

	if (remove_untrack)
	{
		// git clean -d -f, 删除没有add到暂存区的文件
		git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
		opts.checkout_strategy = GIT_CHECKOUT_REMOVE_UNTRACKED;
		error = git_checkout_index(repo, NULL, &opts);
	}

	return error >= 0;
}

static bool __stdcall fetch_cb(::TransferProgress* stats, void* payload)
{
	TransferProgress tmp;
	tmp.total_objects = stats->total_objects;
	tmp.indexed_objects = stats->indexed_objects;
	tmp.received_objects = stats->received_objects;
	tmp.local_objects = stats->local_objects;
	tmp.total_deltas = stats->total_deltas;
	tmp.indexed_deltas = stats->indexed_deltas;
	tmp.received_bytes = stats->received_bytes;

	auto func = reinterpret_cast<FetchCallbackFunction*>(payload);
	return (*func)(tmp);
}
// Reviser ====================================================
Reviser::Reviser()
{
	impl = new Impl();
}
Reviser::~Reviser()
{
	delete impl;
}
void Reviser::setWorkDir(std::string s)
{
	impl->setWorkDir(std::move(s));
}
void Reviser::setRemoteSiteName(std::string s)
{
	impl->setRemoteSiteName(std::move(s));
}
void Reviser::setRemoteSiteURL(std::string s)
{
	impl->setRemoteSiteURL(std::move(s));
}
void Reviser::setRemoteBranch(std::string s)
{
	impl->setRemoteBranch(std::move(s));
}
void Reviser::addIgnore(const std::string& s)
{
	Check(impl->addIgnore(s));
}

void Reviser::open()
{
	Check(impl->open());
}
void Reviser::close()
{
	Check(impl->close());
}

void Reviser::sync(bool remove_untrack) // 与远程一致
{
	Check(impl->sync(remove_untrack));
}
void Reviser::fetch(FetchCallbackFunction callback) // 取得远程信息
{
	Check(impl->fetch(fetch_cb, std::addressof(callback)));
}

std::string Reviser::getWorkDirVersionMessage() // 得到本地版本信息
{
	char buf[1024] = { 0 };
	Check(impl->getWorkDirVersionMessage(buf, sizeof(buf) - 1));
	return buf;
}

std::string Reviser::getRemoteVersionMessage()  // 得到远程版本信息
{
	char buf[1024] = { 0 };
	Check(impl->getRemoteVersionMessage(buf, sizeof(buf) - 1));
	return buf;
}

static bool __stdcall Reviser_MessageCallback(const char* str, void* param)
{
	auto msgList = (MessageList*)param;
	msgList->push_back(str);
	return true;
}

static bool __stdcall Reviser_DiffCallback(const char* file, FileComparationDelta delta, void* param)
{
	auto diffList = (FileComparationList*)param;

	FileComparationItem item;
	item.delta = delta;
	item.file = file;

	diffList->push_back(item);
	return true;
}

MessageList Reviser::getDifferentVersionMessage()  // 得到远程与本地之间的版本差异信息
{
	MessageList result;
	// git log HEAD..ccs/master --oneline 显示版本变化
	Check(impl->getDifferentVersionMessage(Reviser_MessageCallback, &result));
	return result;
}

FileComparationList Reviser::getDifferentFiles() // 得到远程与本地之间有变化的文件（格式：[delta]:[file]）
{
	FileComparationList result;
	Check(impl->getDifferentFiles(Reviser_DiffCallback, &result));
	return result;
}