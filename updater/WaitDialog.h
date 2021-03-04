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
		bool enableCancelButton{ true };
		bool cancelled{ false }; // user clicked cancel button
		void setCompleted()
		{
			pos = 999;
		}
		void setMarquee()
		{
			pos = 0;
		}
	};

	struct WaitArgsWithMutex : WaitArgs
	{
		mutable std::mutex mutex; // should lock before modify	
		void setError(std::wstring errStr)
		{
			std::scoped_lock sl{ mutex };
			contantText = std::move(errStr);
			status = WaitDialog::Status::Error;
			enableCancelButton = true;
		}
	};

	static std::wstring to_wstring(std::string_view s, UINT code_page = CP_ACP);

	static bool Show(WaitArgsWithMutex& args);

	template<class Func>
	static decltype(auto) makeWaitDialogProc(const Func& func) {
		return [&func](WaitDialog::WaitArgsWithMutex& args) {
			try {
				decltype(auto) res = func(args);
				args.setCompleted();
				return res;
			}
			catch (const std::exception& ex) {
				args.setError(to_wstring(ex.what()));
				throw;
			}
			catch (...) {
				args.setError(L"An unexpected error has occurred");
				throw;
			}
		};
	}

	template<class Func>
	static auto ShowAsync(const Func& func, WaitArgsWithMutex& args)
	{
		auto fut = std::async(std::launch::async, makeWaitDialogProc(func), std::ref(args));
		Show(args);
		args.cancelled = true;
		return fut.get();
	}
};