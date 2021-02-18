#pragma once
#include <functional>
#include <mutex>
#include <future>
class WaitDialog
{
public:
	enum class Status {Normal, Error};

	struct WaitArgs
	{
		int pos{ 0 }; // <=0 is MARQUEE, >100 is done, 0~100 is progress
		std::wstring instructionText;
		std::wstring contantText;
		std::wstring title;
		Status status{ Status::Normal };
		bool enable_cancel_button{ true };
		bool cancelled{ false }; // user clicked cancel button
	};

	struct WaitArgsWithMutex
	{
		mutable std::mutex mutex; // should lock before modify
		WaitArgs args;
	};

	static bool WaitProgress(WaitArgsWithMutex& args);

	template<class T>
	static T WaitProgress(std::function<T (WaitArgsWithMutex&)> async_proc, WaitArgsWithMutex& args)
	{
		auto fut = std::async(std::launch::async, async_proc, std::ref(args));
		if (!WaitProgress(args)) args.args.cancelled = true;
		return fut.get();
	}
};