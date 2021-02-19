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

	struct WaitArgsWithMutex : WaitArgs
	{
		mutable std::mutex mutex; // should lock before modify		
	};

	static bool Show(WaitArgsWithMutex& args);

	template<class Func>
	static auto ShowAsync(Func&& func, WaitArgsWithMutex& args)
	{
		auto fut = std::async(std::launch::async, std::forward<Func>(func), std::ref(args));
		if (!Show(args)) args.cancelled = true;
		return fut.get();
	}
};