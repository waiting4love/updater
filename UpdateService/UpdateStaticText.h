#pragma once
#include <atlwin.h>
#include <atlapp.h>
#include <atlctrls.h>
#include <atlgdi.h>
#include <memory>
#include <functional>
#include <chrono>
#include "UpdateServiceExports.h"

class UpdateTextCore;
enum { MSG_VERSIONINFO_RECEVICED = WM_USER + 100, MSG_UPDATE_POS, MSG_ACTIVATE};

class UpdateStaticText
	:public CWindowImpl<UpdateStaticText, WTL::CStatic, CWinTraits<WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY, WS_EX_TRANSPARENT>>
{
public:
	CContainedWindow Parent{this, 100};
	BEGIN_MSG_MAP(UpdateStaticText)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnClick)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(MSG_VERSIONINFO_RECEVICED, OnVersionInfoReceived)
		ALT_MSG_MAP(100)
		MESSAGE_HANDLER(WM_SIZE, OnParentSize)
	END_MSG_MAP()
	LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnClick(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnParentSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnVersionInfoReceived(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	void OnFinalMessage(_In_ HWND /*hWnd*/) override;

	UpdateStaticText();
	~UpdateStaticText() final;

	VersionMessage GetLatestMessage() const;
	void SetShowingHandler(std::function<std::wstring()> msgToText);
	void SetTextColor(COLORREF color);
	// 如果req for exit存在且被调用，在10秒内退出就启用更新。
	void EnableShowBoxOnClick(bool enable, std::function<void(void)> request_exit);
	void EnableAutoSize(bool enable);
	void SetAlignment(Align H, Align V);
	void SetAnchor(UINT, int left, int top, int right, int bottom);
	void EnableManageUpdateInstance(bool enable);
	// 是否在退出时更新，如果通过对话框更新的，则在退出时检查时间
	void EnablePerformUpdateOnExit(bool enable);
	void SetFont(int nPointSize, LPCTSTR lpszFaceName);
	bool UpdateLayout(SIZE size); // 更新大小，如果没有改变返回false
	SIZE CalcTextSize(HDC hdc, wchar_t c, LPCWSTR text, RECT* rcFlag, RECT* rcText);
	bool IsAutoSize() const;
	bool HasAnchor(UINT) const;
	void InvalidateEx() const;
	void RefreshText(LPCTSTR ws);

private:
	UpdateTextCore* core;
};

class UpdateTextWin :
	public CWindowImpl<UpdateTextWin, CWindow, CWinTraits<WS_POPUP | WS_VISIBLE, WS_EX_LAYERED | WS_EX_NOACTIVATE >>
{
public:
	enum {PARENT_ALT_ID = 100};
	CContainedWindow Parent{ this, PARENT_ALT_ID };
	BEGIN_MSG_MAP(UpdateTextWin)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_ACTIVATE, OnActivate)
		MESSAGE_HANDLER(WM_ACTIVATEAPP, OnActivateApp)
		MESSAGE_HANDLER(MSG_VERSIONINFO_RECEVICED, OnVersionInfoReceived)
		MESSAGE_HANDLER(MSG_UPDATE_POS, OnParentSize)
		MESSAGE_HANDLER(MSG_ACTIVATE, OnActivate)
		MESSAGE_HANDLER(messageOfRequireExit, OnExitRequired)
	ALT_MSG_MAP(PARENT_ALT_ID)
		MESSAGE_HANDLER(WM_WINDOWPOSCHANGED, OnParentSize)
		MESSAGE_HANDLER(WM_ACTIVATEAPP, OnActivateApp)
	END_MSG_MAP()
	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnActivate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnActivateApp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnParentSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnVersionInfoReceived(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnExitRequired(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	void OnFinalMessage(HWND) override;
	UpdateTextWin();
	~UpdateTextWin() final;

	VersionMessage GetLatestMessage() const;
	void SetShowingHandler(std::function<std::wstring()> msgToText);
	void SetTextColor(COLORREF bkColor, COLORREF textColor);
	void SetFont(int nPointSize, LPCTSTR lpszFaceName);
	void SetAnchor(UINT, int left, int top, int right, int bottom);
	void EnableShowBoxOnClick(bool enable, std::function<void(void)> request_exit);
	void EnablePerformUpdateOnExit(bool enable);
	void SetTransparent(bool);
	void EnableManageUpdateInstance(bool enable);

private:
	UINT messageOfRequireExit{ WM_DESTROY }; // 初始状态随便用一个前面用过的，等启动后会得到正确的ID
	UpdateTextCore* core;	
	bool UpdateLayout(SIZE size);
	void RefreshText(LPCTSTR ws);
	void TryRemindUpdate();
};