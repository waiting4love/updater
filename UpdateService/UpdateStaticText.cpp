#include "pch.h"
#include "UpdateStaticText.h"
#include "UpdateService.h"
#include "UpdateServiceExports.h"
#include "StringAlgo.h"
#include <algorithm>

wchar_t GetIconChar(VersionMessage msg)
{
	static const WCHAR chNewVersion = 0xf1;
	static const WCHAR chError = 0xfb;
	static const WCHAR chLatest = 0xfc;
	static const WCHAR chNothing = 0xb1;

	WCHAR c = chLatest;
	if (VersionMessage_IsError(msg))
	{
		c = chError;
	}
	else if (VersionMessage_IsNewVersionReady(msg))
	{
		c = chNewVersion;
	}
	else if (VersionMessage_IsNothing(msg))
	{
		c = chNothing;
	}
	return c;
}

bool UpdateStaticText::HasAnchor(UINT anchor) const
{
	return (m_anchor & anchor) == anchor;
}

bool UpdateStaticText::UpdateLayout(SIZE size)
{
	// 1. 得到上级大小
	RECT parentBound;
	RECT curBound;
	auto parent = GetParent();
	parent.GetWindowRect(&parentBound);
	GetWindowRect(&curBound);
	RECT resultBound = curBound;
	// 2. 如果同时有左右锚点，则左右确定左右，如果只有一边锚点，另一边按size
	if (HasAnchor(ANCHOR_LEFT | ANCHOR_RIGHT))
	{
		resultBound.left = parentBound.left + m_offsetFromEdge.left;
		resultBound.right = parentBound.right + m_offsetFromEdge.right;
	}
	else if (HasAnchor(ANCHOR_LEFT))
	{
		resultBound.left = parentBound.left + m_offsetFromEdge.left;
		resultBound.right = resultBound.left + size.cx;
	}
	else if (HasAnchor(ANCHOR_RIGHT))
	{
		resultBound.right = parentBound.right + m_offsetFromEdge.right;
		resultBound.left = resultBound.right - size.cx;
	}
	else
	{
		// 如果没有指定锚点，则直接按size设置大小
		resultBound.right = resultBound.left + size.cx;
	}

	if (HasAnchor(ANCHOR_TOP | ANCHOR_RIGHT))
	{
		resultBound.top = parentBound.top + m_offsetFromEdge.top;
		resultBound.bottom = parentBound.bottom + m_offsetFromEdge.bottom;
	}
	else if (HasAnchor(ANCHOR_TOP))
	{
		resultBound.top = parentBound.top + m_offsetFromEdge.top;
		resultBound.bottom = resultBound.top + size.cy;
	}
	else if (HasAnchor(ANCHOR_BOTTOM))
	{
		resultBound.bottom = parentBound.bottom + m_offsetFromEdge.bottom;
		resultBound.top = resultBound.bottom - size.cy;
	}
	else
	{
		resultBound.bottom = resultBound.top + size.cy;
	}

	if (memcmp(&resultBound, &curBound, sizeof(RECT)) != 0)
	{
		parent.ScreenToClient(&resultBound);
		MoveWindow(&resultBound);
		parent.InvalidateRect(&resultBound);
		return true;
	}
	return false;
}

SIZE UpdateStaticText::CalcTextSize(HDC hdc, wchar_t c, LPCWSTR text, RECT* prcFlag, RECT* prcText)
{
	SIZE size = { 0 };
	RECT rcFlag = { 0 };
	RECT rcText = { 0 };
	
	CDCHandle dc{ hdc };
	if (text[0] != 0)
	{
		dc.SetTextColor(m_clText);
		dc.SetBkMode(TRANSPARENT);
		if (m_ftText.IsNull()) SetFont(110, _T("Tahoma"));

		SIZE sizeFlag = { 0 };
		if (c != 0)
		{
			dc.SelectFont(m_ftWingdings);
			dc.GetTextExtent(&c, 1, &sizeFlag);
			//RECT rcMax = { 0, 0, 1000, 500 };
			rcFlag.right = rcFlag.left + sizeFlag.cx;
			rcFlag.bottom = rcFlag.top + sizeFlag.cy;
		}

		dc.SelectFont(m_ftText);
		rcText.left = rcFlag.right + sizeFlag.cx / 4;
		rcText.bottom = rcText.top + sizeFlag.cy;
		//rcText.right = 
		dc.DrawText(text, -1, &rcText, DT_SINGLELINE | DT_BOTTOM | DT_CALCRECT);

		size.cx = rcText.right;
		size.cy = std::max<>(sizeFlag.cy, rcText.bottom);
	}

	if (prcFlag) *prcFlag = rcFlag;
	if (prcText) *prcText = rcText;
	return size;
}

LRESULT UpdateStaticText::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dc{m_hWnd};

	RECT rcFlag = { 0 };
	RECT rcText = { 0 };

	wchar_t c = GetIconChar(m_latestMsg.get());

	TCHAR buf[256] = { 0 };
	GetWindowText(buf, 256);

	if(SIZE size = CalcTextSize(dc, c, buf, &rcFlag, &rcText); !IsAutoSize() || !UpdateLayout(size))
	{
		RECT rcClient;
		GetClientRect(&rcClient);
		// 画图，按Align画在指定的角落
		int offsetX = 0;
		int offsetY = 0;
		if (m_alignHori == Align::Far)
		{
			offsetX = rcClient.right - rcClient.left - size.cx;
		}
		if (m_alignVert == Align::Far)
		{
			offsetY = rcClient.bottom - rcClient.top - size.cy;
		}
		::OffsetRect(&rcFlag, offsetX, offsetY);
		::OffsetRect(&rcText, offsetX, offsetY);
		dc.SelectFont(m_ftWingdings);
		dc.DrawText(&c, 1, &rcFlag, DT_SINGLELINE | DT_VCENTER);
		dc.SelectFont(m_ftText);
		dc.DrawText(buf, -1, &rcText, DT_SINGLELINE | DT_BOTTOM);
	}

	return 0;
}

LRESULT UpdateStaticText::OnVersionInfoReceived(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	auto msg = Update_GetVersionMessage();
	m_latestMsg = decltype(m_latestMsg)(msg, &VersionMessage_Destory);

	std::wstring ws;
	if (ShowingHandler)
	{
		ws = ShowingHandler();
	}
	else
	{
		if (VersionMessage_IsNewVersionReady(msg))
		{
			int len = VersionMessage_GetRemoteMessage(msg, nullptr, 0);
			std::wstring s(len, ' ');
			VersionMessage_GetRemoteMessage(msg, s.data(), s.length());
			if (auto pos = s.find_first_of('\n'); pos != std::wstring::npos)
			{
				s.erase(pos);
			}
			
			s = s + L" is available!";
			ws = to_wstring(s);
		}
	}
	
	SetWindowText(ws.c_str());
	if (IsAutoSize())
	{
		wchar_t c = GetIconChar(m_latestMsg.get());
		SIZE size = CalcTextSize(CWindowDC(*this), c, ws.c_str(), nullptr, nullptr);
		UpdateLayout(size);
	}
	else
	{
		RECT rc = { 0 };
		GetClientRect(&rc);
		UpdateLayout({ rc.right, rc.bottom });
	}

	return 0;
}

bool UpdateStaticText::IsAutoSize() const
{
	return m_bAutoSize && !HasAnchor(ANCHOR_LEFT|ANCHOR_TOP|ANCHOR_TOP|ANCHOR_BOTTOM);
}

LRESULT UpdateStaticText::OnParentSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;
	if (m_anchor != ANCHOR_NONE)
	{
		RECT rc = { 0 };
		GetClientRect(&rc);
		UpdateLayout({rc.right, rc.bottom});
	}
	return 0;
}

LRESULT UpdateStaticText::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (Parent.m_hWnd != NULL)
	{
		Parent.UnsubclassWindow();
	}
	return 0;
}

void UpdateStaticText::OnFinalMessage(HWND)
{
	if (m_bManageUpdateInstance)
	{
		Update_StopWatch();
	}

	// 如果有新版本
	if (Update_IsNewVersionReady())
	{
		if (clock_type::now() - m_clkClickUpdateButton < std::chrono::seconds{ 10 })
		{
			// 最后一次点Update按钮10秒内，更新并重启
			Update_Perform(true);
		}
		else if (m_bPerformUpdateOnExit)
		{
			// 如果有更新标志，更新并退出
			Update_Perform(false);
		}
	}

	if(m_bManageUpdateInstance)
	{
		Update_Uninitialize();
	}
	delete this;
}

VersionMessage UpdateStaticText::GetLatestMessage() const
{
	return m_latestMsg.get();
}
void UpdateStaticText::SetColor(COLORREF color)
{
	m_clText = color;
	Invalidate();
}
void UpdateStaticText::EnableShowBoxOnClick(bool enable, std::function<void(void)> request_exit)
{
	m_bClickToShow = enable;
	m_funcRequestExit = std::move(request_exit);
}
void UpdateStaticText::EnableAutoSize(bool enable)
{
	m_bAutoSize = enable;
	Invalidate();
}

void UpdateStaticText::UpdateOffsetFromEdge()
{
	RECT rcThis;
	RECT rcParent;
	GetWindowRect(&rcThis);
	GetParent().GetWindowRect(&rcParent);
	m_offsetFromEdge.left = rcThis.left - rcParent.left;
	m_offsetFromEdge.right = rcThis.right - rcParent.right;
	m_offsetFromEdge.top = rcThis.top - rcParent.top;
	m_offsetFromEdge.bottom = rcThis.bottom - rcParent.bottom;
}

void UpdateStaticText::SetAlignment(Align H, Align V)
{
	m_alignHori = H;
	m_alignVert = V;
	Invalidate();
}

void UpdateStaticText::SetAnchor(UINT anchor)
{
	m_anchor = anchor;
	UpdateOffsetFromEdge();
	if (anchor != ANCHOR_NONE && Parent.m_hWnd == nullptr)
	{
		Parent.SubclassWindow(GetParent());
	}
}

void UpdateStaticText::EnableManageUpdateInstance(bool enable)
{
	m_bManageUpdateInstance = enable;
}

void UpdateStaticText::EnablePerformUpdateOnExit(bool enable)
{
	m_bPerformUpdateOnExit = enable;
}

void UpdateStaticText::SetFont(int nPointSize, LPCTSTR lpszFaceName)
{
	m_ftText.CreatePointFont(nPointSize, lpszFaceName);
	m_ftWingdings.CreatePointFont(nPointSize+10, _T("Wingdings"));
}

LRESULT UpdateStaticText::OnClick(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!m_bClickToShow)
	{
		bHandled = FALSE;
		return 0;
	}
	
	if (auto msg = GetLatestMessage(); VersionMessage_IsNewVersionReady(msg) && m_funcRequestExit)
	{
		//if (LPCTSTR buttons[] = { L"Update", L"Later" }; 1 == VersionMessage_ShowBox(msg, GetParent(), NULL, buttons, 2))
		if (LPCTSTR buttons[] = { L"Update" }; 1 == VersionMessage_ShowBox(msg, GetParent(), NULL, buttons, 1))
		{
			m_clkClickUpdateButton = clock_type::now();
			m_funcRequestExit();
		}
		else
		{
			m_clkClickUpdateButton = clock_type::time_point{};
		}
	}
	else
	{
		VersionMessage_ShowBox(msg, GetParent(), NULL, NULL, 0);
	}
	return 0;
}