#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include "resource.h"

INT_PTR CALLBACK DlgProc_About(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG: return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: case IDCANCEL: EndDialog(hDlg, LOWORD(wParam)); return TRUE;
		}
	case WM_NOTIFY: {
		switch (((LPNMHDR)lParam)->code) {
		case NM_CLICK: case NM_RETURN: {
			const PNMLINK pNMLink = (PNMLINK)lParam;
			if (((LPNMHDR)lParam)->hwndFrom == GetDlgItem(hDlg, IDC_SYSLINK))
				ShellExecuteW(NULL, L"open", pNMLink->item.szUrl, NULL, NULL, SW_SHOW);
		}	break;
		}
	}	break;
	}
	return FALSE;
}