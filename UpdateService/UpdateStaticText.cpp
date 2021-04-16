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

	VersionMessage GetLatestMessage() const
	{
		return m_latestMsg.get();
	}
	void SetTextColor(COLORREF color)
	{
		m_clText = color;
	}
	COLORREF GetTextColor() const {
		return m_clText;
	}
	void SetBackColor(COLORREF color)
	{
		m_clBack = color;
	}
	COLORREF GetBackColor() const {
		return m_clBack;
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
		m_ftText = std::make_unique<Gdiplus::Font>( lpszFaceName, nPointSize / 10.0F );
		m_ftIcon = std::make_unique<Gdiplus::Font>(_T("Segoe UI Symbol"), nPointSize / 10.0F + 1.0F);
	}

	bool IsAutoSize() const
	{
		return m_bAutoSize && !HasAnchor(ANCHOR_LEFT | ANCHOR_TOP | ANCHOR_TOP | ANCHOR_BOTTOM);
	}
	void Initialize()
	{
		gdiplus.Init();
		SetFont(80, L"Segoe UI");
		m_latestInst.Acquire();
	}

	void Uninitialize()
	{
		m_latestInst.Release();
		m_ftIcon.reset();
		m_ftText.reset();
		gdiplus.Uninit();

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

	static Gdiplus::RectF CreateRectF(const RECT& rc)
	{
		return {
			FLOAT(rc.left), FLOAT(rc.top), FLOAT(rc.right - rc.left), FLOAT(rc.bottom - rc.top)
		};
	}

	static RECT GetRect(const Gdiplus::RectF& rc)
	{
		return {
			(LONG)std::floorf(rc.GetLeft()),
			(LONG)std::floorf(rc.GetTop()),
			(LONG)std::ceilf(rc.GetRight()),
			(LONG)std::ceilf(rc.GetBottom())
		};
	}

	// size, rcFlag, rcText
	std::tuple<SIZE, RECT, RECT> CalcTextSize(HDC hdc, wchar_t c, LPCWSTR text)
	{
		SIZE size = { 0 };
		RECT rcFlag = { 0 };
		RECT rcText = { 0 };

		if (text[0] != 0)
		{
			Gdiplus::Graphics gr{ hdc };
			SIZE sizeFlag = { 0 };
			if (c != 0)
			{
				Gdiplus::RectF re;
				gr.MeasureString(&c, 1, m_ftIcon.get(), Gdiplus::PointF{}, &re);
				rcFlag = GetRect(re);
				sizeFlag = {
					rcFlag.right - rcFlag.left,
					rcFlag.bottom - rcFlag.top };
			}

			Gdiplus::RectF re;
			gr.MeasureString(text, lstrlen(text), m_ftText.get(), Gdiplus::PointF{}, &re);
			rcText = GetRect(re);
			::OffsetRect(&rcText, rcFlag.right, rcFlag.top + (rcFlag.bottom - rcFlag.top - (rcText.bottom - rcText.top)));

			size.cx = rcText.right;
			size.cy = std::max<>(sizeFlag.cy, rcText.bottom);
		}

		return std::make_tuple(size, rcFlag, rcText);
	}

	void PaintText(HDC dc, LPCTSTR text, const RECT& rcClient)
	{
		wchar_t c = GetIconChar(m_latestMsg.get());
		auto [size, rcFlag, rcText] = CalcTextSize(dc, c, text);

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
		Gdiplus::Graphics gr(dc);
		//gr.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

		Gdiplus::Color cl;
		cl.SetFromCOLORREF(m_clText);

		Gdiplus::SolidBrush brs{ cl };
		Gdiplus::StringFormat fmt{ Gdiplus::StringFormatFlagsNoWrap| Gdiplus::StringFormatFlagsNoClip };
		fmt.SetAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
		fmt.SetLineAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);

		gr.DrawString(&c, 1, m_ftIcon.get(), CreateRectF(rcFlag), &fmt, &brs);
		gr.DrawString(text, lstrlen(text), m_ftText.get(), CreateRectF(rcText), &fmt, &brs);
	}

	void PaintBack(HDC dc, LPCTSTR buf, const RECT& rcClient)
	{
		Gdiplus::Graphics gr(dc);
		Gdiplus::RectF rc{ 0, 0, (float)rcClient.right, (float)rcClient.bottom };

		Gdiplus::Color cl1 = Gdiplus::Color::MakeARGB(192, GetRValue(m_clBack), GetGValue(m_clBack), GetBValue(m_clBack));
		Gdiplus::Color cl2(0, cl1.GetR(), cl1.GetG(), cl1.GetB());
		Gdiplus::LinearGradientBrush linearGradientBrush{ rc, cl1, cl2, Gdiplus::LinearGradientMode::LinearGradientModeHorizontal };
		Gdiplus::Color Colors[] = { cl2, cl1, cl1, cl2 };
		Gdiplus::REAL pos[] = { 0.0F, 0.1F, 0.9F, 1.0F };
		linearGradientBrush.SetInterpolationColors(Colors, pos, 4);
		gr.FillRectangle(&linearGradientBrush, rc);
	}

	void PaintBack2(HDC dc, LPCTSTR buf, const RECT& rcClient)
	{
		Gdiplus::Graphics gr(dc);
		Gdiplus::RectF rc{ 0, 0, (float)rcClient.right, (float)rcClient.bottom };
		Gdiplus::GraphicsPath path;
		Gdiplus::RectF rc2 = rc;
		rc2.Height *= 1.5F;
		path.AddEllipse(rc2);

		Gdiplus::Color cl1 = Gdiplus::Color::MakeARGB(218, GetRValue(m_clBack), GetGValue(m_clBack), GetBValue(m_clBack));
		Gdiplus::Color cl2(0, cl1.GetR(), cl1.GetG(), cl1.GetB());
		Gdiplus::PathGradientBrush pthGrBrush(&path);

		Gdiplus::REAL fac[] = { 0.0F, 1.0F, 1.0F };
		Gdiplus::REAL pos[] = { 0.0F, 0.5F, 1.0F };
		pthGrBrush.SetBlend(fac, pos, 3);
		pthGrBrush.SetCenterColor(cl1);
		Gdiplus::Color colors[] = { cl2 };
		int count = 1;
		pthGrBrush.SetSurroundColors(colors, &count);
		gr.FillRectangle(&pthGrBrush, rc);
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
			ws = std::move(s);
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
	std::unique_ptr<Gdiplus::Font> m_ftIcon;
	std::unique_ptr<Gdiplus::Font> m_ftText;
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
	COLORREF m_clBack{ RGB(255, 255, 255) };
	RECT m_offsetFromEdge{ 0 };
	GdiPlusIniter gdiplus;
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
	core->PaintText(dc.m_hDC, buf, rcClient);
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
void UpdateStaticText::SetTextColor(COLORREF color)
{
	core->SetTextColor(color);
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
	HWND ParentWnd = GetParent();
	core->EnableManageUpdateInstance(true);
	core->Initialize();
	core->SetAnchor(ANCHOR_TOP | ANCHOR_RIGHT);
	core->SetAlignment(Align::Center, Align::Center);
	core->SetBackColor(0xffffff);
	core->SetTextColor(0);
	core->EnableAutoSize(true);
	core->EnableShowBoxOnClick(true, [this, ParentWnd]() { ::SendMessage(::GetAncestor(ParentWnd, GA_ROOTOWNER), WM_CLOSE, 0, 0); });

	int rightEdge = int(CWindowDC(ParentWnd).GetDeviceCaps(LOGPIXELSX) * 1.5);

	RECT rc = {
		0, 3, -rightEdge, 0
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
	return 0;
}

LRESULT UpdateTextWin::OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	POINT pt;
	::GetCursorPos(&pt);
	if (DragDetect(m_hWnd, pt))
	{
		ReleaseCapture();
		::SendMessage(GetParent(), WM_SYSCOMMAND, SC_MOVE|HTCAPTION, 0);
	}
	else
	{
		core->PerformClick(GetParent());
	}
	return 0;
}

LRESULT UpdateTextWin::OnActivate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (uMsg == WM_ACTIVATE)
	{
		WORD status = LOWORD(wParam);
		if (/*status == WA_ACTIVE || */status == WA_CLICKACTIVE)
		{
			PostMessage(MSG_ACTIVATE);
		}
	}
	else if(uMsg == MSG_ACTIVATE)
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
	bHandled = FALSE;
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
		// 如果向上超过屏幕，在一定范围内就适当下移
		if (resultBound.top < 0 && resultBound.bottom > 0)
		{
			::OffsetRect(&resultBound, 0, -resultBound.top + 3); 
		}
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
	HDC m_hDCOriginal;
	CBitmap m_bmp;
	HBITMAP m_hBmpOld{nullptr};

	CNoCopyMemoryDC(HDC hDC, const RECT& rcPaint) : m_hDCOriginal(hDC)
	{
		CreateCompatibleDC(m_hDCOriginal);
		ATLASSERT(m_hDC != NULL);
		m_bmp.CreateCompatibleBitmap(m_hDCOriginal, rcPaint.right - rcPaint.left, rcPaint.bottom - rcPaint.top);
		ATLASSERT(m_bmp.m_hBitmap != NULL);
		m_hBmpOld = SelectBitmap(m_bmp);
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

	if (IsRectEmpty(&rcWin))
	{
		if (IsWindowVisible()) ShowWindow(SW_HIDE);
	}
	else
	{
		if (!IsWindowVisible()) ShowWindow(SW_SHOWNOACTIVATE);
	}

	POINT ptWinPos = { rcWin.left,rcWin.top };
	SIZE sizeWindow = {
		rcWin.right - rcWin.left,
		rcWin.bottom - rcWin.top
	};
	POINT ptSrc = { 0,0 };
	RECT rcClient = {
		0,0,sizeWindow.cx,sizeWindow.cy
	};

	BLENDFUNCTION bf = { 0 };
	bf.BlendOp = 0;
	bf.BlendFlags = 0;
	bf.AlphaFormat = AC_SRC_ALPHA;
	bf.SourceConstantAlpha = 255;

	TCHAR buf[256] = { 0 };
	GetWindowText(buf, 256);

	CNoCopyMemoryDC bmpDC(dc, rcClient);
	core->PaintBack(bmpDC, buf, rcClient);
	core->PaintText(bmpDC, buf, rcClient);
	
	::UpdateLayeredWindow(m_hWnd, dc, &ptWinPos, &sizeWindow, bmpDC, &ptSrc, 0,
		&bf, ULW_ALPHA);
}

void UpdateTextWin::SetShowingHandler(std::function<std::wstring()> msgToText)
{
	core->SetShowingHandler(std::move(msgToText));
	PostMessage(MSG_VERSIONINFO_RECEVICED);
}
void UpdateTextWin::SetTextColor(COLORREF bkColor, COLORREF textColor)
{
	core->SetBackColor(bkColor);
	core->SetTextColor(textColor);
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
	SetTransparent(!enable);
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

void UpdateTextWin::EnableManageUpdateInstance(bool enable)
{
	core->EnableManageUpdateInstance(enable);
}