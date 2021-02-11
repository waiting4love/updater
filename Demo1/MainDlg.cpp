// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "MainDlg.h"
#include "../UpdateService/Exports.h"
#pragma comment(lib, "UpdateService.lib")

void __stdcall VersionInfoReceived(void* param)
{
	auto dlg = (CMainDlg*)param;
	dlg->OnVersionInformationReceived();
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

	Update_Initialize();
	Update_StartWatch(60'000, VersionInfoReceived, this);
	return TRUE;
}

void CMainDlg::OnVersionInformationReceived()
{	
	PostMessage(WM_USER+1);
}

LRESULT CMainDlg::OnVersionReceived(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	char s[2048] = { 0 };
	auto vi = Update_GetVersionMessage();
	if (VersionMessage_IsError(vi))
	{
		VersionMessage_GetErrorMessage(vi, s, sizeof(s));
	}
	else if (VersionMessage_IsNewVersionReady(vi))
	{
		VersionMessage_GetRemoteMessage(vi, s, sizeof(s));
	}
	else if (VersionMessage_IsNothing(vi))
	{
		lstrcpyA(s, "No information received");
	}
	else
	{
		VersionMessage_GetLocalMessage(vi, s, sizeof(s));
	}
	VersionMessage_Destory(vi);
	::SetDlgItemTextA(m_hWnd, IDC_STATIC_VERSION, s);

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
	Update_StopWatch();
	if (Update_IsNewVersionReady())
	{
		Update_Perform(false);
	}
	EndDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Update_StopWatch();
	EndDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	Update_Uninitialize();
	return 0;
}