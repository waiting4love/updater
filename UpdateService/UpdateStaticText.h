#pragma once
#include <atlwin.h>
#include <atlapp.h>
#include <atlctrls.h>
#include <atlgdi.h>
#include <memory>
#include <functional>
#include "Exports.h"

class UpdateStaticText
	:public CWindowImpl<UpdateStaticText, WTL::CStatic, CWinTraits<WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY, WS_EX_TRANSPARENT>>
{
public:
	using base = CWindowImpl<UpdateStaticText, WTL::CStatic, CWinTraits<WS_CHILD | WS_VISIBLE, WS_EX_TRANSPARENT>>;
	enum { MSG_VERSIONINFO_RECEVICED = WM_USER + 100 };
	enum class Align { Near, Far };

	std::function<std::wstring ()> ShowingHandler;
	CContainedWindow Parent{this, 100};
	BEGIN_MSG_MAP(UpdateStaticText)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnClick)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		//MESSAGE_HANDLER(OCM__BASE + WM_SIZE, OnParentSize)
		MESSAGE_HANDLER(MSG_VERSIONINFO_RECEVICED, OnVersionInfoReceived)
		ALT_MSG_MAP(100)
		MESSAGE_HANDLER(WM_SIZE, OnParentSize)
	END_MSG_MAP()
	LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnClick(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnParentSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnVersionInfoReceived(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	void OnFinalMessage(_In_ HWND /*hWnd*/) override;
	VersionMessage GetLatestMessage() const;
	void SetColor(COLORREF color);
	void EnableShowBoxOnClick(bool enable, std::function<void(void)> request_exit);
	void EnableAutoSize(bool enable);
	void SetAlignment(Align H, Align V);
	void UpdateOffsetFromEdge();
	void EnableManageUpdateInstance(bool enable);
	void SetFont(int nPointSize, LPCTSTR lpszFaceName);
private:
	WTL::CFont m_ftWingdings;
	WTL::CFont m_ftText;
	std::unique_ptr<std::remove_pointer_t<VersionMessage>, decltype(&VersionMessage_Destory)> m_latestMsg{ nullptr, &VersionMessage_Destory };
	bool m_bAutoSize{ false };
	bool m_bClickToShow{ false };
	std::function<void(void)> m_funcRequestExit{ nullptr };
	bool m_bManageUpdateInstance{ false };
	Align m_alignHori{ Align::Near };
	Align m_alignVert{ Align::Near };
	COLORREF m_clText{ RGB(0, 192, 0) };
	RECT m_offsetFromEdge{ 0 };
};

