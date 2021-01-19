// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "MainDlg.h"

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// center the dialog on the screen
	CenterWindow();

	// set icons
	HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	SetIcon(hIconSmall, FALSE);

	UpdateService.setCheckInterval(60000);
	UpdateService.setVersionReceivedHandler([this]() { OnVersionInformationReceived(); });
	UpdateService.start();
	return TRUE;
}

void CMainDlg::OnVersionInformationReceived()
{	
	PostMessage(WM_USER+1);
}

LRESULT CMainDlg::OnVersionReceived(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	auto vi = UpdateService.getVersionInfo();
	String s;
	if (vi.isError())
	{
		s = vi.ErrorMessage;
	}
	else if (vi.isNewVersionReady())
	{
		for (const auto& i : vi.Status.Remote)
		{
			s = s + i + "\n";
		}
	}
	else
	{
		if (!vi.Status.Local.empty())
			s = "This is latest version: " + vi.Status.Local.front();
		else
			s = "No information received";
	}
	::SetDlgItemTextA(m_hWnd, IDC_STATIC_VERSION, s.c_str());
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
	// TODO: Add validation code 
	EndDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	UpdateService.stop();
	if (UpdateService.isNewVersionReady())
	{
		UpdateService.doUpdate();
	}
	return 0;
}