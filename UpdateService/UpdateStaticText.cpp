#include "pch.h"
#include "UpdateStaticText.h"
#include "UpdateService.h"
#include "Exports.h"
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

void UpdateStaticText::Resize(SIZE size)
{
	RECT rcWindow;
	GetWindowRect(&rcWindow);
	if (rcWindow.right - rcWindow.left != size.cx || rcWindow.bottom - rcWindow.top != size.cy)
	{
		// 还要看一下对齐方式
		if (m_alignHori == Align::Near)
		{
			rcWindow.right = rcWindow.left + size.cx;
		}
		else
		{
			rcWindow.left = rcWindow.right - size.cx;
		}

		if (m_alignVert == Align::Near)
		{
			rcWindow.bottom = rcWindow.top + size.cy;
		}
		else
		{
			rcWindow.top = rcWindow.bottom - size.cy;
		}

		auto par = GetParent();
		par.ScreenToClient(&rcWindow);
		MoveWindow(&rcWindow);
		par.InvalidateRect(&rcWindow);
	}
}

LRESULT UpdateStaticText::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dc{m_hWnd};

	SIZE size = { 0 };
	RECT rcLeft = { 0 };
	RECT rcText = { 0 };

	TCHAR buf[256] = { 0 };
	wchar_t c = GetIconChar(m_latestMsg.get());

	GetWindowText(buf, 256);
	if (buf[0] != 0)
	{
		dc.SetTextColor(m_clText);
		dc.SetBkMode(TRANSPARENT);
		if (m_ftText.IsNull()) SetFont(110, _T("Arial"));

		dc.SelectFont(m_ftWingdings);

		SIZE sizeFlag = { 0 };
		dc.GetTextExtent(&c, 1, &sizeFlag);

		RECT rcMax = {0, 0, 1000, 500};
		rcLeft = rcMax;
		rcLeft.right = rcLeft.left + sizeFlag.cx;
		rcLeft.bottom = rcLeft.top + sizeFlag.cy;
		dc.SelectFont(m_ftText);

		rcText = rcMax;
		rcText.left = rcLeft.right + sizeFlag.cx/4;
		rcText.bottom = rcText.top + sizeFlag.cy;
		dc.DrawText(buf, -1, &rcText, DT_SINGLELINE | DT_BOTTOM | DT_CALCRECT);

		size.cx = rcText.right;
		size.cy = std::max<>(sizeFlag.cy, rcText.bottom);
	}

	size.cx++;
	size.cy++;

	RECT rcWindow;
	GetWindowRect(&rcWindow);
	// 如果大小不对，则改大小先
	if (m_bAutoSize && (rcWindow.right - rcWindow.left != size.cx || rcWindow.bottom - rcWindow.top != size.cy))
	{
		Resize(size);
	}
	else
	{
		// 画图，按Align画在指定的角落
		int offsetX = 0;
		int offsetY = 0;
		if (m_alignHori == Align::Far)
		{
			offsetX = rcWindow.right - rcWindow.left - size.cx;
		}
		if (m_alignVert == Align::Far)
		{
			offsetY = rcWindow.bottom - rcWindow.top - size.cy;
		}
		::OffsetRect(&rcLeft, offsetX, offsetY);
		::OffsetRect(&rcText, offsetX, offsetY);
		dc.SelectFont(m_ftWingdings);
		dc.DrawText(&c, 1, &rcLeft, DT_SINGLELINE | DT_VCENTER);
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
	RECT rc;
	GetClientRect(&rc);
	if (::IsRectEmpty(&rc))
	{
		Resize({ 1,1 });
	}
	Invalidate();

	return 0;
}

LRESULT UpdateStaticText::OnParentSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;
	if (m_alignHori == Align::Far || m_alignVert == Align::Far)
	{
		auto parent = GetParent();
		RECT curBound;
		RECT parentBound;
		GetWindowRect(&curBound);
		parent.GetWindowRect(&parentBound);
		int offsetX = 0;
		int offsetY = 0;
		if (m_alignHori == Align::Far)
		{
			offsetX = parentBound.right +  m_offsetFromEdge.right - curBound.right;
		}
		if (m_alignVert == Align::Far)
		{
			offsetY = parentBound.bottom + m_offsetFromEdge.bottom - curBound.bottom;
		}
		::OffsetRect(&curBound, offsetX, offsetY);
		parent.ScreenToClient(&curBound);
		MoveWindow(&curBound);
		parent.InvalidateRect(&curBound);
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

	UpdateOffsetFromEdge();

	if (m_alignHori == Align::Far || m_alignVert == Align::Far)
	{
		if (Parent.m_hWnd == NULL)
		{
			Parent.SubclassWindow(GetParent());
		}
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