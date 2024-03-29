#include "stdafx.h"
#include "StringAlgo.h"
#include "WaitDialog.h"
#include <CommCtrl.h>

#pragma comment(lib, "Comctl32.lib")

//std::wstring WaitDialog::to_wstring(std::string_view s, UINT code_page)
//{
//	std::wstring ws;
//	ws.resize(::MultiByteToWideChar(code_page, 0, s.data(), s.length(), nullptr, 0));
//	::MultiByteToWideChar(code_page, 0, s.data(), s.length(), ws.data(), ws.size());
//	return ws;
//}

struct WaitArgsPair
{
	WaitDialog::WaitArgsWithMutex& Current;
	WaitDialog::WaitArgs Prev;
	bool First{ true };

	explicit WaitArgsPair(WaitDialog::WaitArgsWithMutex& args_w_mutex)
		:Current(args_w_mutex)
	{
	}

	[[nodiscard]] static bool inRange(int pos)
	{
		return pos > 0 && pos < 100;
	}

	[[nodiscard]] bool isCompleted() const
	{
		return Current.pos > 100;
	}

	void updateTaskDialog(HWND hWnd)
	{
		if (First)
		{
			::SendMessage(hWnd, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(0, 100));
		}

		std::scoped_lock sl{ Current.mutex };
		auto& args = (const WaitDialog::WaitArgs&)Current;

		bool changed = false;
#define IS_DIFF(x) bool c = args.x != Prev.x; (changed=changed||c), (First || c)

		if(IS_DIFF(enableCancelButton)) {
			::SendMessage(hWnd, TDM_ENABLE_BUTTON, IDCANCEL, args.enableCancelButton?TRUE:FALSE);
		}

		if (IS_DIFF(instructionText)) {
			::SendMessage(hWnd, TDM_SET_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, (LPARAM)args.instructionText.c_str());
		}

		if (IS_DIFF(contantText)) {
			::SendMessage(hWnd, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)args.contantText.c_str());
		}

		if (IS_DIFF(title)) {
			::SetWindowText(hWnd, args.title.c_str());
		}

		if (IS_DIFF(pos))
		{
			if (!inRange(args.pos))
			{
				if (First || inRange(Prev.pos))
				{
					::SendMessage(hWnd, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
					::SendMessage(hWnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 50); // start
				}
			}
			else
			{
				if(First || !inRange(Prev.pos)) ::SendMessage(hWnd, TDM_SET_MARQUEE_PROGRESS_BAR, FALSE, 0);
				::SendMessage(hWnd, TDM_SET_PROGRESS_BAR_POS, std::min<WPARAM>(100, args.pos), 0L);
			}
		}

		if (IS_DIFF(status))
		{
			::SendMessage(hWnd, TDM_SET_PROGRESS_BAR_STATE, args.status == WaitDialog::Status::Error ? PBST_ERROR : PBST_NORMAL, 0L);
			::SendMessage(hWnd, TDM_UPDATE_ICON, TDIE_ICON_MAIN, args.status == WaitDialog::Status::Error ? (LPARAM)TD_ERROR_ICON : 0);
		}

		First = false;
		if(changed) Prev = args;
	}
};

HRESULT CALLBACK TaskDialogCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData)
{
	auto data = (WaitArgsPair*)lpRefData;
	switch (uMsg)
	{
	case TDN_DIALOG_CONSTRUCTED:
		break;
	case TDN_CREATED:
		data->updateTaskDialog(hWnd);
		break;
	case TDN_BUTTON_CLICKED:
		if (wParam == IDCANCEL) data->Current.cancelled = true;
		break;
	case TDN_RADIO_BUTTON_CLICKED:
		break;
	case TDN_HYPERLINK_CLICKED:
		break;
	case TDN_EXPANDO_BUTTON_CLICKED:
		break;
	case TDN_VERIFICATION_CLICKED:
		break;
	case TDN_HELP:
		break;
	case TDN_TIMER:
		data->updateTaskDialog(hWnd);
		if (data->isCompleted())
		{
			::SendMessage(hWnd, TDM_CLICK_BUTTON, IDCANCEL, 0);
		}
		break;
	case TDN_NAVIGATED:
		break;
	case TDN_DESTROYED:
		break;
	default:
		break;
	}

	return S_OK;
}

bool WaitDialog::Show(WaitArgsWithMutex& args)
{
	WaitArgsPair data{ args };

	TASKDIALOGCONFIG tdc = { 0 };
	tdc.cbSize = sizeof(TASKDIALOGCONFIG);
	tdc.dwFlags = TDF_SHOW_MARQUEE_PROGRESS_BAR | TDF_CALLBACK_TIMER;
	tdc.dwCommonButtons = TDCBF_CANCEL_BUTTON;
	tdc.lpCallbackData = (LONG_PTR)&data;
	tdc.pfCallback = TaskDialogCallback;
	auto res = TaskDialogIndirect(&tdc, nullptr, nullptr, nullptr);
	return SUCCEEDED(res);
}
