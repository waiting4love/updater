#include "pch.h"
#include "UpdateStaticText.h"
#include "UpdateService.h"
#include "UpdateServiceExports.h"
#include "StringAlgo.h"
#include <gdiplus.h>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")

class GdiPlusIniter
{
public:
	ULONG_PTR gpToken;

	void Init()
	{
		Gdiplus::GdiplusStartupInput gsi;
		Gdiplus::GdiplusStartup(&gpToken, &gsi, NULL);

	}
	void Uninit()
	{
		Gdiplus::GdiplusShutdown(gpToken);
	}
};

GdiPlusIniter g_gdi;

class UpdateTextCore
{
public:
	static wchar_t GetIconChar(VersionMessage msg)
	{
		static const WCHAR chNewVersion = L'⬆';
		static const WCHAR chError = L'✖';
		static const WCHAR chLatest = L'✔';
		static const WCHAR chNothing = L'⮂';
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

	wchar_t GetIconChar()
	{
		return GetIconChar(m_latestMsg.get());
	}

	bool HasAnchor(UINT anchor) const
	{
		return (m_anchor & anchor) == anchor;
	}
	// new bound and is changed
	std::pair<RECT, bool> CalcLayoutBound(const RECT& parentBound, const RECT& curBound, SIZE size)
	{
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

		if (HasAnchor(ANCHOR_TOP | ANCHOR_BOTTOM))
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

		return std::make_pair(resultBound, memcmp(&resultBound, &curBound, sizeof(RECT)) != 0);
	}

	// size, rcFlag, rcText
	std::tuple<SIZE, RECT, RECT> CalcTextSize(HDC hdc, wchar_t c, LPCWSTR text)
	{
		SIZE size = { 0 };
		RECT rcFlag = { 0 };
		RECT rcText = { 0 };

		CDCHandle dc{ hdc };
		if (text[0] != 0)
		{
			if (m_ftText.IsNull()) SetFont(110, _T("Tahoma"));

			SIZE sizeFlag = { 0 };
			if (c != 0)
			{
				dc.SelectFont(m_ftIcon);
				dc.GetTextExtent(&c, 1, &sizeFlag);
				rcFlag.right = rcFlag.left + sizeFlag.cx;
				rcFlag.bottom = rcFlag.top + sizeFlag.cy;
			}

			dc.SelectFont(m_ftText);
			rcText.left = rcFlag.right + sizeFlag.cx / 4;
			rcText.bottom = rcText.top + sizeFlag.cy;
			dc.DrawText(text, -1, &rcText, DT_SINGLELINE | DT_BOTTOM | DT_CALCRECT);

			size.cx = rcText.right;
			size.cy = std::max<>(sizeFlag.cy, rcText.bottom);
		}

		return std::make_tuple(size, rcFlag, rcText);
	}

	VersionMessage GetLatestMessage() const
	{
		return m_latestMsg.get();
	}
	void SetColor(COLORREF color)
	{
		m_clText = color;
	}
	COLORREF GetColor() const {
		return m_clText;
	}
	void EnableShowBoxOnClick(bool enable, std::function<void(void)> request_exit)
	{
		m_bClickToShow = enable;
		m_funcRequestExit = std::move(request_exit);
	}
	void EnableAutoSize(bool enable)
	{
		m_bAutoSize = enable;
	}

	void UpdateOffsetFromEdge(const RECT &rcParent, const RECT &rcThis)
	{
		m_offsetFromEdge.left = rcThis.left - rcParent.left;
		m_offsetFromEdge.right = rcThis.right - rcParent.right;
		m_offsetFromEdge.top = rcThis.top - rcParent.top;
		m_offsetFromEdge.bottom = rcThis.bottom - rcParent.bottom;
	}

	void SetOffsetFromEdge(const RECT& edge)
	{
		m_offsetFromEdge = edge;
	}

	void SetAlignment(Align H, Align V)
	{
		m_alignHori = H;
		m_alignVert = V;
	}

	void SetAnchor(UINT anchor)
	{
		m_anchor = anchor;
	}

	void EnableManageUpdateInstance(bool enable)
	{
		m_bManageUpdateInstance = enable;
	}

	void EnablePerformUpdateOnExit(bool enable)
	{
		m_bPerformUpdateOnExit = enable;
	}

	void SetFont(int nPointSize, LPCTSTR lpszFaceName)
	{
		m_ftText.CreatePointFont(nPointSize, lpszFaceName);
		m_ftIcon.CreatePointFont(nPointSize, _T("Segoe UI Symbol"));
	}
	HFONT GetFont()
	{
		return m_ftText;
	}
	bool IsAutoSize() const
	{
		return m_bAutoSize && !HasAnchor(ANCHOR_LEFT | ANCHOR_TOP | ANCHOR_TOP | ANCHOR_BOTTOM);
	}
	void Initialize()
	{
		m_latestInst.Acquire();
	}

	void Uninitialize()
	{
		m_latestInst.Release();

		if (m_bManageUpdateInstance)
		{
			Update_StopWatch();
		}

		// 如果有新版本
		if (Update_IsNewVersionReady())
		{
			if (clock_type::now() - m_clkClickUpdateButton < std::chrono::seconds{ 10 })
			{
				// 最后一次点Update按钮10秒内，更新并重启，且无视是否最后的程序
				Update_Perform(true);
			}
			else if (m_bPerformUpdateOnExit && !m_latestInst.Exists())
			{
				// 如果有更新标志，更新并退出
				Update_Perform(false);
			}
		}

		if (m_bManageUpdateInstance)
		{
			Update_Uninitialize();
		}
	}

	void Paint(CDCHandle dc, LPCTSTR buf, const RECT& rcClient)
	{
		wchar_t c = GetIconChar(m_latestMsg.get());
		auto [size, rcFlag, rcText] = CalcTextSize(dc, c, buf);

		// 画图，按Align画在指定的角落
		int offsetX = 0;
		int offsetY = 0;
		if (m_alignHori == Align::Far)
		{
			offsetX = rcClient.right - rcClient.left - size.cx;
		}
		else if (m_alignHori == Align::Center)
		{
			offsetX = (rcClient.right - rcClient.left - size.cx) / 2;
		}

		if (m_alignVert == Align::Far)
		{
			offsetY = rcClient.bottom - rcClient.top - size.cy;
		}
		else if (m_alignVert == Align::Center)
		{
			offsetY = (rcClient.bottom - rcClient.top - size.cy)/2;
		}

		::OffsetRect(&rcFlag, offsetX, offsetY);
		::OffsetRect(&rcText, offsetX, offsetY);
		auto savedDc = dc.SaveDC();
		dc.SetTextColor(m_clText);
		dc.SetBkMode(TRANSPARENT);
		dc.SelectFont(m_ftIcon);
		dc.DrawText(&c, 1, &rcFlag, DT_SINGLELINE | DT_VCENTER);
		dc.SelectFont(m_ftText);
		dc.DrawText(buf, -1, &rcText, DT_SINGLELINE | DT_BOTTOM);
		dc.RestoreDC(savedDc);
	}

	void UpdateMsg()
	{
		auto msg = Update_GetVersionMessage();
		m_latestMsg = decltype(m_latestMsg)(msg, &VersionMessage_Destory);
	}

	void SetShowingHandler(std::function<std::wstring()> msgToText)
	{
		m_funcShowingHandler = std::move(msgToText);
	}

	std::wstring GetDefaultMsgText()
	{
		std::wstring ws;
		if (auto msg = GetLatestMessage(); VersionMessage_IsNewVersionReady(msg))
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

		return ws;
	}

	std::wstring GetMsgText()
	{
		if (m_funcShowingHandler)
		{
			return m_funcShowingHandler();
		}
		else
		{
			return GetDefaultMsgText();
		}
	}

	UINT GetAnchor() const
	{
		return m_anchor;
	}

	bool PerformClick(HWND parent)
	{
		if (!m_bClickToShow)
			return false;

		if (auto msg = GetLatestMessage(); VersionMessage_IsNewVersionReady(msg) && m_funcRequestExit)
		{
			if (LPCTSTR buttons[] = { L"Update" }; 1 == VersionMessage_ShowBox(msg, parent, NULL, buttons, 1))
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
			VersionMessage_ShowBox(msg, parent, NULL, NULL, 0);
		}
		return true;
	}
private:
	LatestInstance m_latestInst;
	WTL::CFont m_ftIcon;
	WTL::CFont m_ftText;
	std::unique_ptr<std::remove_pointer_t<VersionMessage>, decltype(&VersionMessage_Destory)> m_latestMsg{ nullptr, &VersionMessage_Destory };
	bool m_bAutoSize{ true };
	bool m_bClickToShow{ true };
	bool m_bPerformUpdateOnExit{ false };
	using clock_type = std::chrono::steady_clock;
	clock_type::time_point m_clkClickUpdateButton;
	std::function<void(void)> m_funcRequestExit{ nullptr };
	std::function<std::wstring()> m_funcShowingHandler{ nullptr };
	bool m_bManageUpdateInstance{ false };
	Align m_alignHori{ Align::Near };
	Align m_alignVert{ Align::Near };
	UINT m_anchor{ ANCHOR_NONE };
	COLORREF m_clText{ RGB(0, 192, 0) };
	RECT m_offsetFromEdge{ 0 };
};

UpdateStaticText::UpdateStaticText()
{
	core = new UpdateTextCore();
}
UpdateStaticText::~UpdateStaticText()
{
	delete core;
}

bool UpdateStaticText::HasAnchor(UINT anchor) const
{
	return core->HasAnchor(anchor);
}

bool UpdateStaticText::UpdateLayout(SIZE size)
{
	RECT parentBound;
	RECT curBound;
	auto parent = GetParent();
	parent.GetClientRect(&parentBound);
	GetWindowRect(&curBound);
	parent.ScreenToClient(&curBound);

	auto [resultBound, changed] = core->CalcLayoutBound(parentBound, curBound, size);
	if(changed)
	{
		SetWindowPos(parent, &resultBound, SWP_NOACTIVATE | SWP_NOZORDER);
		parent.Invalidate();
		return true;
	}
	return false;
}

void UpdateStaticText::InvalidateEx() const
{
	RECT curBound;
	auto parent = GetParent();
	GetWindowRect(&curBound);
	parent.ScreenToClient(&curBound);
	parent.InvalidateRect(&curBound);
	::InvalidateRect(m_hWnd, NULL, TRUE);
}

SIZE UpdateStaticText::CalcTextSize(HDC hdc, wchar_t c, LPCWSTR text, RECT* prcFlag, RECT* prcText)
{
	auto [size, rcFlag, rcText] = core->CalcTextSize(hdc, c, text);
	if (prcFlag) *prcFlag = rcFlag;
	if (prcText) *prcText = rcText;
	return size;
}

LRESULT UpdateStaticText::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	core->Initialize();
	return true;
}

LRESULT UpdateStaticText::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	TCHAR buf[256] = { 0 };
	GetWindowText(buf, 256);

	RECT rcClient;
	GetClientRect(&rcClient);

	CPaintDC dc{ m_hWnd };
	core->Paint(dc.m_hDC, buf, rcClient);
	return 0;
}

LRESULT UpdateStaticText::OnVersionInfoReceived(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	core->UpdateMsg();
	std::wstring ws = core->GetMsgText();
	
	TCHAR buf[256] = { 0 };
	GetWindowText(buf, 256);
	SetWindowText(ws.c_str());

	if (buf != ws)
	{
		RefreshText(ws.c_str());
	}

	return 0;
}

void UpdateStaticText::RefreshText(LPCTSTR ws)
{
	if (IsAutoSize())
	{
		wchar_t c = UpdateTextCore::GetIconChar(core->GetLatestMessage());
		SIZE size = CalcTextSize(CWindowDC(*this), c, ws, nullptr, nullptr);
		UpdateLayout(size);
	}
	else
	{
		RECT rc = { 0 };
		GetClientRect(&rc);
		UpdateLayout({ rc.right, rc.bottom });
	}
	InvalidateEx();
}

bool UpdateStaticText::IsAutoSize() const
{
	return core->IsAutoSize();
}

LRESULT UpdateStaticText::OnParentSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;
	if (core->GetAnchor() != ANCHOR_NONE)
	{
		RECT rc = { 0 };
		GetClientRect(&rc);
		UpdateLayout({rc.right, rc.bottom});
		InvalidateEx();
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
	core->Uninitialize();
	delete this;
}

VersionMessage UpdateStaticText::GetLatestMessage() const
{
	return core->GetLatestMessage();
}
void UpdateStaticText::SetColor(COLORREF color)
{
	core->SetColor(color);
	Invalidate();
}
void UpdateStaticText::EnableShowBoxOnClick(bool enable, std::function<void(void)> request_exit)
{
	core->EnableShowBoxOnClick(enable, std::move(request_exit));
}
void UpdateStaticText::EnableAutoSize(bool enable)
{
	core->EnableAutoSize(enable);
	if (enable)
	{
		TCHAR buf[256] = { 0 };
		GetWindowText(buf, 256);
		RefreshText(buf);
	}
}

void UpdateStaticText::SetAlignment(Align H, Align V)
{
	core->SetAlignment(H, V);
	Invalidate();
}

void UpdateStaticText::SetAnchor(UINT anchor, int left, int top, int right, int bottom)
{
	core->SetAnchor(anchor);
	core->SetOffsetFromEdge({ left, top, right, bottom });
	if (anchor != ANCHOR_NONE && Parent.m_hWnd == nullptr)
	{
		Parent.SubclassWindow(GetParent());
	}
}

void UpdateStaticText::EnableManageUpdateInstance(bool enable)
{
	core->EnableManageUpdateInstance(enable);
}

void UpdateStaticText::EnablePerformUpdateOnExit(bool enable)
{
	core->EnablePerformUpdateOnExit(enable);
}

void UpdateStaticText::SetFont(int nPointSize, LPCTSTR lpszFaceName)
{
	core->SetFont(nPointSize, lpszFaceName);
	InvalidateEx();
}

void UpdateStaticText::SetShowingHandler(std::function<std::wstring()> msgToText)
{
	core->SetShowingHandler(std::move(msgToText));
	PostMessage(MSG_VERSIONINFO_RECEVICED);
}

LRESULT UpdateStaticText::OnClick(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if(!core->PerformClick(GetParent()))
	{
		bHandled = FALSE;
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////
// 默认对齐上右标题框位置，向左移空出三个按钮位置, 自动大小
// 字体与标题栏相同，颜色与标题栏相同，背景为黄色透明
// 显示当前版本或都是新版本
// 点击显示，退出更新
UpdateTextWin::UpdateTextWin()
{
	core = new UpdateTextCore();
}
UpdateTextWin::~UpdateTextWin()
{
	delete core;
}
LRESULT UpdateTextWin::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	g_gdi.Init();

	HWND ParentWnd = GetParent();
	core->EnableManageUpdateInstance(true);
	core->Initialize();
	core->SetFont(80, L"Segoe UI");
	core->SetAnchor(ANCHOR_TOP | ANCHOR_RIGHT);
	core->EnableAutoSize(true);
	core->EnableShowBoxOnClick(true, [this, ParentWnd]() { ::SendMessage(::GetAncestor(ParentWnd, GA_ROOTOWNER), WM_CLOSE, 0, 0); });

	//TITLEBARINFOEX infoEx = {0};
	//infoEx.cbSize = sizeof(infoEx);
	//::SendMessage(ParentWnd, WM_GETTITLEBARINFOEX, 0, LPARAM(&infoEx));
	//int mostLeft = infoEx.rcTitleBar.right;
	//for (int i : {2,3,4,5}) // min, max, help, close
	//{
	//	if (!::IsRectEmpty(&infoEx.rgrect[i]))
	//	{
	//		mostLeft = std::min<int>(infoEx.rgrect[i].left, mostLeft);
	//	}
	//}

	//int rightEdge = mostLeft - infoEx.rcTitleBar.right - 5;
	int rightEdge = -145;
	RECT rc = {
		0, 3, rightEdge, 0
	};
	core->SetOffsetFromEdge(rc);
	Parent.SubclassWindow(ParentWnd);

	ShowWindow(SW_SHOW);

	return 0;
}

LRESULT UpdateTextWin::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (Parent.m_hWnd != NULL)
	{
		Parent.UnsubclassWindow();
	}
	g_gdi.Uninit();
	return 0;
}

LRESULT UpdateTextWin::OnClick(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!core->PerformClick(GetParent()))
	{
		bHandled = FALSE;
	}
	return 0;
}

LRESULT UpdateTextWin::OnActivate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	WORD status = LOWORD(wParam);
	if (status == WA_ACTIVE || status == WA_CLICKACTIVE)
	{
		GetParent().SetActiveWindow();
	}
	return 0;
}

void UpdateTextWin::OnFinalMessage(HWND)
{
	core->Uninitialize();
	delete this;
}

LRESULT UpdateTextWin::OnParentSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	RECT rc = { 0 };
	GetClientRect(&rc);
	UpdateLayout({ rc.right, rc.bottom });
	return 0;
}

bool UpdateTextWin::UpdateLayout(SIZE size)
{
	RECT parentBound;
	RECT curBound;
	auto parent = GetParent();
	parent.GetWindowRect(&parentBound);
	GetWindowRect(&curBound);

	auto [resultBound, changed] = core->CalcLayoutBound(parentBound, curBound, size);
	if (changed)
	{
		SetWindowPos(NULL, &resultBound, SWP_NOACTIVATE| SWP_NOZORDER);
		return true;
	}
	return false;
}

LRESULT UpdateTextWin::OnVersionInfoReceived(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	core->UpdateMsg();

	std::wstring ws;
	ws = core->GetMsgText();	

	TCHAR buf[256] = { 0 };
	GetWindowText(buf, 256);
	SetWindowText(ws.c_str());

	if (buf != ws)
	{
		RefreshText(ws.c_str());
	}

	return 0;
}

class CNoCopyMemoryDC : public CDC
{
public:
	// Data members
	HDC m_hDCOriginal;
	RECT m_rcPaint;
	CBitmap m_bmp;
	HBITMAP m_hBmpOld;

	CNoCopyMemoryDC(HDC hDC, const RECT& rcPaint) : m_hDCOriginal(hDC), m_hBmpOld(NULL)
	{
		m_rcPaint = rcPaint;
		CreateCompatibleDC(m_hDCOriginal);
		ATLASSERT(m_hDC != NULL);
		m_bmp.CreateCompatibleBitmap(m_hDCOriginal, m_rcPaint.right - m_rcPaint.left, m_rcPaint.bottom - m_rcPaint.top);
		ATLASSERT(m_bmp.m_hBitmap != NULL);
		m_hBmpOld = SelectBitmap(m_bmp);
		SetViewportOrg(-m_rcPaint.left, -m_rcPaint.top);
	}

	~CNoCopyMemoryDC()
	{
		SelectBitmap(m_hBmpOld);
	}
};

void UpdateTextWin::RefreshText(LPCTSTR ws)
{
	CWindowDC dc(*this);

	if (core->IsAutoSize())
	{
		wchar_t c = UpdateTextCore::GetIconChar(core->GetLatestMessage());		
		auto [size, a, b] = core->CalcTextSize(dc, c, ws);
		size.cx += size.cy*2;
		//size.cy *= 1.5;
		UpdateLayout(size);
	}
	else
	{
		RECT rcWin;
		GetWindowRect(&rcWin);
		SIZE sizeWindow = {
			rcWin.right - rcWin.left,
			rcWin.bottom - rcWin.top
				};
		UpdateLayout(sizeWindow);
	}

	RECT rcWin;
	GetWindowRect(&rcWin);
	POINT ptWinPos = { rcWin.left,rcWin.top };
	SIZE sizeWindow = {
		rcWin.right - rcWin.left,
		rcWin.bottom - rcWin.top
	};
	POINT ptSrc = { 0,0 };
	RECT rcClient = {
		0,0,sizeWindow.cx,sizeWindow.cy
	};

	CNoCopyMemoryDC bmpDC(dc, rcClient);

	BLENDFUNCTION bf = { 0 };
	bf.BlendOp = 0;
	bf.BlendFlags = 0;
	bf.AlphaFormat = AC_SRC_ALPHA;
	bf.SourceConstantAlpha = 255;

	TCHAR buf[256] = { 0 };
	int len = GetWindowText(buf+1, 255);
	buf[0] = core->GetIconChar();
	len++;

	Gdiplus::Graphics gr(bmpDC);
	Gdiplus::RectF rc{ 0, 0, (float)sizeWindow.cx, (float)sizeWindow.cy };

	//Gdiplus::GraphicsPath path;
	//Gdiplus::RectF rc2 = rc;
	//rc2.Height *= 1.5F;
	//path.AddEllipse(rc2);

	//Gdiplus::Color cl1(255, 255, 255, 255);
	//Gdiplus::Color cl2(0, 255, 255, 255);
	//Gdiplus::PathGradientBrush pthGrBrush(&path);
	//pthGrBrush.SetCenterColor(cl1);	
	//Gdiplus::Color colors[] = { cl2 };
	//int count = 1;
	//pthGrBrush.SetSurroundColors(colors, &count);
	//gr.FillRectangle(&pthGrBrush, rc);

	Gdiplus::Color cl1 = Gdiplus::Color::MakeARGB(192, GetRValue(bkColor), GetGValue(bkColor), GetBValue(bkColor));	
	Gdiplus::Color cl2(0, cl1.GetR(), cl1.GetG(), cl1.GetB());
	Gdiplus::LinearGradientBrush linearGradientBrush{ rc, cl1, cl2, Gdiplus::LinearGradientMode::LinearGradientModeHorizontal };
	Gdiplus::Color Colors[] = { cl2, cl1, cl1, cl2 };
	Gdiplus::REAL pos[] = { 0.0F, 0.1F, 0.9F, 1.0F };
	linearGradientBrush.SetInterpolationColors(Colors, pos, 4);
	gr.FillRectangle(&linearGradientBrush, rc);

	Gdiplus::Font ft{ bmpDC, core->GetFont() };
	Gdiplus::Color cl;
	cl.SetFromCOLORREF(textColor);

	Gdiplus::SolidBrush brs{ cl };
	Gdiplus::StringFormat fmt;
	fmt.SetAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
	fmt.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
	gr.DrawString(buf, len, &ft, rc, &fmt, &brs);
	
	::UpdateLayeredWindow(m_hWnd, dc, &ptWinPos, &sizeWindow, bmpDC, &ptSrc, 0,
		&bf, ULW_ALPHA);
}

void UpdateTextWin::SetShowingHandler(std::function<std::wstring()> msgToText)
{
	core->SetShowingHandler(std::move(msgToText));
	PostMessage(MSG_VERSIONINFO_RECEVICED);
}
void UpdateTextWin::SetColor(COLORREF bkColor, COLORREF textColor)
{
	this->bkColor = bkColor;
	this->textColor = textColor;
	SetWindowText(L"");
	PostMessage(MSG_VERSIONINFO_RECEVICED);
}
void UpdateTextWin::SetFont(int nPointSize, LPCTSTR lpszFaceName)
{
	core->SetFont(nPointSize, lpszFaceName);
	SetWindowText(L"");
	PostMessage(MSG_VERSIONINFO_RECEVICED);
}
void UpdateTextWin::SetAnchor(UINT anchor, int left, int top, int right, int bottom)
{
	core->SetAnchor(anchor);
	core->SetOffsetFromEdge({ left, top, right, bottom });
	PostMessage(MSG_UPDATE_POS);
}
void UpdateTextWin::EnableShowBoxOnClick(bool enable, std::function<void(void)> request_exit)
{
	core->EnableShowBoxOnClick(enable, std::move(request_exit));
}
void UpdateTextWin::SetTransparent(bool trans)
{
	if (trans)
		ModifyStyleEx(0, WS_EX_TRANSPARENT);
	else
		ModifyStyleEx(WS_EX_TRANSPARENT, 0);
}
void UpdateTextWin::EnablePerformUpdateOnExit(bool enable)
{
	core->EnablePerformUpdateOnExit(enable);
}
VersionMessage UpdateTextWin::GetLatestMessage() const
{
	return core->GetLatestMessage();
}
