// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "MainDlg.h"
#include <string>
#include <array>
#pragma comment(lib, "UpdateService.lib")

void __stdcall CMainDlg::VersionInfoReceived(void* param)
{
	auto dlg = (CMainDlg*)param;
	dlg->OnVersionInformationReceived();
}

void __stdcall CMainDlg::CloseApp(void* param)
{
	auto dlg = (CMainDlg*)param;
	dlg->EndDialog(0);
}

void __stdcall CMainDlg::MsgToText(VersionMessage msg, void*, wchar_t* text)
{
	if (VersionMessage_IsError(msg))
	{
		VersionMessage_GetErrorMessage(msg, text, MAX_LABLE_LEN);
	}
	else if (VersionMessage_IsNewVersionReady(msg))
	{
		wchar_t buf[2048] = { 0 };
		VersionMessage_GetRemoteMessage(msg, buf, 2048);
		if (auto* ptr = wcschr(buf, L'\n'); ptr) *ptr = 0;
		lstrcpynW(text, buf, MAX_LABLE_LEN);
	}
	else if (VersionMessage_IsNothing(msg))
	{
		lstrcpy(text, _T("Connecting..."));
	}
	else
	{
		wchar_t buf[2048] = { 0 };
		VersionMessage_GetLocalMessage(msg, buf, 2048);
		if (auto* ptr = wcschr(buf, L'\n'); ptr) *ptr = 0;
		lstrcpynW(text, buf, MAX_LABLE_LEN);
	}
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// center the dialog on the screen
	CenterWindow();

	// set icons
	HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	SetIcon(hIconSmall, FALSE);
	
	auto lbl = VersionMessageLabel_Create(m_hWnd, NULL, 1001, true);
	//VersionMessageLabel_SetColor(lbl, ::GetSysColor(COLOR_HOTLIGHT));
	//VersionMessageLabel_SetAlignment(lbl, true, true);
	VersionMessageLabel_SetAnchor(lbl, ANCHOR_BOTTOM|ANCHOR_RIGHT, 0, 0, 0, 0);
	//VersionMessageLabel_EnableAutoSize(lbl, true);
	//VersionMessageLabel_SetFont(lbl, 140, L"Arial");
	//VersionMessageLabel_SetShowingHandler(lbl, LABEL_TEXT_ALLCASE, 0);
	//VersionMessageLabel_EnableShowBoxOnClick(lbl, true, CloseApp, this);
	//VersionMessageLabel_EnableShowBoxOnClick(lbl, true, EXIT_BY_MESSAGE, nullptr);
	// VersionMessageLabel_EnablePerformUpdateOnExit(lbl, true);
	
	//auto w = VersionMessageWin_Create(m_hWnd);
	////VersionMessageWin_SetAnchor(w, ANCHOR_TOP | ANCHOR_RIGHT, 0, -20, -10, 0);
	//VersionMessageWin_SetShowingHandler(w, LABEL_TEXT_ALLCASE, 0);
	////VersionMessageWin_SetTransparent(w, false);
	//Update_SetCheckInterval(1000);
	return TRUE;
}

LRESULT CMainDlg::OnShow(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	auto w = VersionMessageWin_Create(m_hWnd);
	//VersionMessageWin_SetAnchor(w, ANCHOR_TOP | ANCHOR_RIGHT, 0, -20, -10, 0);
	VersionMessageWin_SetShowingHandler(w, LABEL_TEXT_ALLCASE, 0);
	//VersionMessageWin_SetTransparent(w, false);
	//Update_SetCheckInterval(1000);
	return 0;
}

void CMainDlg::OnVersionInformationReceived()
{	
	PostMessage(WM_USER+1);
}

LRESULT CMainDlg::OnVersionReceived(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	//char s[2048] = { 0 };
	//auto vi = Update_GetVersionMessage();
	//if (VersionMessage_IsError(vi))
	//{
	//	VersionMessage_GetErrorMessage(vi, s, sizeof(s));
	//}
	//else if (VersionMessage_IsNewVersionReady(vi))
	//{
	//	VersionMessage_GetRemoteMessage(vi, s, sizeof(s));
	//}
	//else if (VersionMessage_IsNothing(vi))
	//{
	//	lstrcpyA(s, "No information received");
	//}
	//else
	//{
	//	VersionMessage_GetLocalMessage(vi, s, sizeof(s));
	//}
	//VersionMessage_Destory(vi);
	//::SetDlgItemTextA(m_hWnd, IDC_STATIC_VERSION, s);
	//::InvalidateRect(GetDlgItem(1001), NULL, TRUE);

	return 0;
}

LRESULT CMainDlg::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CSimpleDialog<IDD_ABOUTBOX, FALSE> dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	//Update_StopWatch();
	//if (Update_IsNewVersionReady())
	//{
	//	Update_Perform(false);
	//}
	EndDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	//Update_StopWatch();
	EndDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	//Update_Uninitialize();
	return 0;
}

LRESULT CMainDlg::OnVersionClick(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	//auto buttons = std::array{
	//	L"Update", L"Later"
	//};

	//auto msg = Update_GetVersionMessage();
	//int res = VersionMessage_ShowBox(msg, m_hWnd, L"For Demo, No matter click which button", buttons.data(), buttons.size());
	//ATLTRACE("ShowBox Reture:%d", res);
	//VersionMessage_Destory(msg);
	return 0;
}