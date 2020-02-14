/*
Project Name: Simple Paint
Last Update: 2020/02/14

This project is hosted on https://github.com/Hydr10n/Simple-Paint
Copyright (C) Programmer-Yang_Xun@outlook.com. All Rights Reserved.
*/

#include <memory>
#include <vector>
#include <stack>
#include <string>
#include "resource.h"
#include "About.h"
#include "Utilities.h"

#define APP_NAME L"Simple Paint"
#define WINDOW_TITLE_SUFFIX (L" - " APP_NAME)
#define DEFAULT_FILE_TITLE L"Untitled"
#define UNSAVE_FILE_PROMPT L"Do you want to save changes to "
#define SAVE_FILE_FAIL_PROMPT L"Failed to save changes due to the following reason:\n"

#define MAIN_WINDOW_WIDTH 1350
#define MAIN_WINDOW_HEIGHT 850
#define CANVAS_WIDTH 1280
#define CANVAS_HEIGHT 720
#define CANVAS_LEFT 3
#define CANVAS_TOP CANVAS_LEFT
#define CANVAS_PADDING 3
#define CANVAS_MARGIN 7
#define CANVAS_SHADOW_OFFSET 5
#define CANVAS_SHADOW_LENGTH 4
#define PEN_WIDTH_1PX 1
#define PEN_WIDTH_2PX 2
#define PEN_WIDTH_4PX 4
#define PEN_WIDTH_8PX 8
#define ERASER_WIDTH_1PX PEN_WIDTH_1PX
#define ERASER_WIDTH_2PX PEN_WIDTH_2PX
#define ERASER_WIDTH_4PX PEN_WIDTH_4PX
#define ERASER_WIDTH_8PX PEN_WIDTH_8PX

using std::auto_ptr;
using std::vector;
using std::stack;
using std::wstring;
using std::to_wstring;

struct PIXEL {
	DWORD dwDIBSectionIndex;
	RGBQUAD Rgbq;
};

struct PartialBitmap {
	SIZE Size;
	vector<PIXEL> Pixels;
};

enum class PaintingTools { Pen = IDM_PEN, Eraser = IDM_ERASER, Fill = IDM_FILL, ColorPicker = IDM_COLORPICKER };

BOOL bFileSaved = TRUE;
int iDPI = USER_DEFAULT_SCREEN_DPI, iPenWidth = PEN_WIDTH_8PX, iEraserWidth = ERASER_WIDTH_8PX, iActualMargin;
PaintingTools paintingTool = PaintingTools::Pen, previousPaintingTool = paintingTool;
COLORREF penColor = RGB(0, 128, 192);
SIZE currentScroll, maxBitmapSize, canvasSize;
HWND hWnd_StatusBar;
HMENU hMenu;
HDC hDC_Canvas, hDC_Memory;
stack<PartialBitmap> undoHistory, redoHistory;

LRESULT CALLBACK WndProc_Main(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc_PaintView(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc_Canvas(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	SIZE size = { MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT };
	HDC hDC = GetDC(NULL);
	if (hDC) {
		SetProcessDPIAware();
		iDPI = GetDeviceCaps(hDC, LOGPIXELSX);
		size = { Scale(size.cx, iDPI), Scale(size.cy, iDPI) };
		ReleaseDC(NULL, hDC);
	}
	WNDCLASSW wndClass = { 0 };
	wndClass.hInstance = hInstance;
	wndClass.lpfnWndProc = WndProc_Main;
	wndClass.lpszClassName = L"SimplePaint";
	wndClass.lpszMenuName = MAKEINTRESOURCEW(IDR_MENU);
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = SYS_WHITE_BRUSH;
	RegisterClassW(&wndClass);
	STARTUPINFO startupInfo;
	GetStartupInfoW(&startupInfo);
	HWND hWnd = CreateWindowW(wndClass.lpszClassName, (wstring(DEFAULT_FILE_TITLE) + WINDOW_TITLE_SUFFIX).c_str(),
		WS_OVERLAPPEDWINDOW,
		startupInfo.dwFlags & STARTF_USEPOSITION ? startupInfo.dwX : (GetSystemMetrics(SM_CXSCREEN) - size.cx) / 2,
		startupInfo.dwFlags & STARTF_USEPOSITION ? startupInfo.dwY : (GetSystemMetrics(SM_CYSCREEN) - size.cy) / 2,
		size.cx, size.cy,
		NULL, NULL, hInstance, NULL);
	ShowWindow(hWnd, nShowCmd);
	UpdateWindow(hWnd);
	HACCEL hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDR_ACCELERATOR));
	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0))
		if (!TranslateAcceleratorW(hWnd, hAccel, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	DestroyAcceleratorTable(hAccel);
	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc_Main(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static BOOL bFileEverSaved;
	static LONG lStatusBarHeight;
	static WCHAR szFileName[MAX_PATH] = DEFAULT_FILE_TITLE;
	static HWND hWnd_PaintView;
	static HBRUSH hBrush_Background;
	switch (uMsg) {
	case WM_CREATE: {
		const HINSTANCE hInstance = ((LPCREATESTRUCTW)lParam)->hInstance;
		const INT uParts[] = { Scale(200, iDPI), Scale(400, iDPI) };
		iActualMargin = Scale(CANVAS_MARGIN + CANVAS_PADDING, iDPI);
		hWnd_StatusBar = CreateWindowW(STATUSCLASSNAMEW, NULL,
			WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
			0, 0, 0, 0,
			hWnd, NULL, hInstance, NULL);
		RECT statusBarRect;
		GetWindowRect(hWnd_StatusBar, &statusBarRect);
		lStatusBarHeight = statusBarRect.bottom - statusBarRect.top;
		SendMessageW(hWnd_StatusBar, SB_SETPARTS, _countof(uParts), (LPARAM)uParts);
		WNDCLASSW wndClass = { 0 };
		wndClass.hInstance = hInstance;
		wndClass.lpszClassName = L"PaintView";
		wndClass.lpfnWndProc = WndProc_PaintView;
		wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		hBrush_Background = wndClass.hbrBackground = CreateSolidBrush(RGB(220, 230, 240));
		RegisterClassW(&wndClass);
		hWnd_PaintView = CreateWindowW(wndClass.lpszClassName, NULL,
			WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL,
			0, 0, 0, 0,
			hWnd, NULL, hInstance, NULL);
		hMenu = GetMenu(hWnd);
		CheckMenuRadioItem(hMenu, IDM_PEN, IDM_COLORPICKER, IDM_PEN, MF_BYCOMMAND);
		CheckMenuRadioItem(hMenu, IDM_PENSIZE_1PX, IDM_PENSIZE_8PX, IDM_PENSIZE_8PX, MF_BYCOMMAND);
		CheckMenuRadioItem(hMenu, IDM_ERASERSIZE_1PX, IDM_ERASERSIZE_8PX, IDM_ERASERSIZE_8PX, MF_BYCOMMAND);
	}	break;
	case WM_GETMINMAXINFO: ((LPMINMAXINFO)lParam)->ptMinTrackSize = { Scale(230, iDPI), Scale(230, iDPI) }; return 0;
	case WM_SIZE: {
		SetWindowPos(hWnd_StatusBar, NULL, 0, 0, 0, 0, SWP_NOZORDER);
		SetWindowPos(hWnd_PaintView, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam) - lStatusBarHeight, SWP_NOZORDER);
	}	break;
	case WM_COMMAND: {
		const WORD wParamLow = LOWORD(wParam);
		switch (wParamLow) {
		case IDA_NEW: {
			if (!bFileSaved)
				switch (MessageBoxW(hWnd, (wstring(UNSAVE_FILE_PROMPT) + szFileName + L'?').c_str(), L"Confirm", MB_YESNOCANCEL)) {
				case IDYES: {
					SendMessageW(hWnd, WM_COMMAND, IDA_SAVE, 0);
					if (bFileSaved)
						goto discard;
				}	break;
				case IDNO: goto discard;
				}
			else {
			discard:;
				RECT rect = { 0, 0, canvasSize.cx, canvasSize.cy };
				FillRect(hDC_Canvas, &rect, SYS_WHITE_BRUSH);
				FillRect(hDC_Memory, &rect, SYS_WHITE_BRUSH);
				undoHistory = decltype(undoHistory)();
				redoHistory = decltype(redoHistory)();
				EnableMenuItem(hMenu, IDM_UNDO, MF_DISABLED);
				EnableMenuItem(hMenu, IDM_REDO, MF_DISABLED);
			}
		}	break;
		case IDA_NEWWINDOW: {
			WCHAR szProgramFileName[MAX_PATH];
			GetModuleFileNameW(NULL, szProgramFileName, _countof(szProgramFileName));
			RECT rect;
			GetWindowRect(hWnd, &rect);
			STARTUPINFOW startupInfo = { sizeof(startupInfo) };
			startupInfo.dwFlags = STARTF_USEPOSITION;
			startupInfo.dwX = rect.left;
			startupInfo.dwY = rect.top;
			PROCESS_INFORMATION processInfo;
			CreateProcessW(szProgramFileName, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}	break;
		case IDA_SAVE: {
			if (!bFileEverSaved)
				goto saveAs;
			if (SaveDCAs24bitBitmapFile(szFileName, hDC_Memory, canvasSize.cx, canvasSize.cy))
				bFileSaved = bFileEverSaved = TRUE;
			else
				MessageBoxW(hWnd,
				(wstring(SAVE_FILE_FAIL_PROMPT) + SysErrorMsg(GetLastError()).GetMsg()).c_str(), NULL,
					MB_OK | MB_ICONERROR);
		}	break;
		case IDA_SAVEAS: {
		saveAs:;
			WCHAR szFileTitle[_countof(szFileName)];
			OPENFILENAMEW openFileName = { sizeof(openFileName) };
			openFileName.hwndOwner = hWnd;
			openFileName.lpstrFile = szFileName;
			openFileName.lpstrFileTitle = szFileTitle;
			openFileName.nMaxFileTitle = openFileName.nMaxFile = _countof(szFileName);
			openFileName.lpstrFilter = L"24-bit Bitmap (*.bmp)\0";
			openFileName.lpstrDefExt = L"bmp";
			openFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;
			if (GetSaveFileNameW(&openFileName))
				if (SaveDCAs24bitBitmapFile(szFileName, hDC_Memory, canvasSize.cx, canvasSize.cy)) {
					bFileSaved = bFileEverSaved = TRUE;
					PathRemoveExtensionW(szFileTitle);
					SetWindowTextW(hWnd, (szFileTitle + wstring(WINDOW_TITLE_SUFFIX)).c_str());
				}
				else
					MessageBoxW(hWnd,
					(wstring(SAVE_FILE_FAIL_PROMPT) + SysErrorMsg(GetLastError()).GetMsg()).c_str(), NULL,
						MB_OK | MB_ICONERROR);
			else
				return 1;
		}	break;
		case IDM_EXIT: PostMessage(hWnd, WM_CLOSE, 0, 0); break;
		case IDM_PEN: case IDM_ERASER: case IDM_FILL: case IDM_COLORPICKER: {
			CheckMenuRadioItem(hMenu, IDM_PEN, IDM_COLORPICKER, wParamLow, MF_BYCOMMAND);
			paintingTool = (PaintingTools)wParamLow;
			if (wParamLow != IDM_COLORPICKER)
				previousPaintingTool = paintingTool;
		}	break;
		case IDM_PENSIZE_1PX: iPenWidth = PEN_WIDTH_1PX; goto pensize_8px;
		case IDM_PENSIZE_2PX: iPenWidth = PEN_WIDTH_2PX; goto pensize_8px;
		case IDM_PENSIZE_4PX: iPenWidth = PEN_WIDTH_4PX; goto pensize_8px;
		case IDM_PENSIZE_8PX: iPenWidth = PEN_WIDTH_8PX;
		pensize_8px:; CheckMenuRadioItem(hMenu, IDM_PENSIZE_1PX, IDM_PENSIZE_8PX, wParamLow, MF_BYCOMMAND); break;
		case IDM_ERASERSIZE_1PX: iEraserWidth = ERASER_WIDTH_1PX; goto erasersize_8px;
		case IDM_ERASERSIZE_2PX: iEraserWidth = ERASER_WIDTH_2PX; goto erasersize_8px;
		case IDM_ERASERSIZE_4PX: iEraserWidth = ERASER_WIDTH_4PX; goto erasersize_8px;
		case IDM_ERASERSIZE_8PX: iEraserWidth = ERASER_WIDTH_8PX;
		erasersize_8px:; CheckMenuRadioItem(hMenu, IDM_ERASERSIZE_1PX, IDM_ERASERSIZE_8PX, wParamLow, MF_BYCOMMAND); break;
		case IDM_COLOR: {
			static COLORREF custColors[16] = { penColor };
			CHOOSECOLORW chooseColor = { sizeof(chooseColor) };
			chooseColor.hwndOwner = hWnd;
			chooseColor.lpCustColors = custColors;
			chooseColor.rgbResult = penColor;
			chooseColor.Flags = CC_FULLOPEN | CC_RGBINIT;
			if (ChooseColorW(&chooseColor))
				penColor = chooseColor.rgbResult;
		}	break;
		case IDM_ABOUT: DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ABOUT), hWnd, DlgProc_About); break;
		default: PostMessageW(GetDlgItem(hWnd_PaintView, ID_CANVAS), uMsg, wParam, lParam); break;
		}
	}	break;
	case WM_CLOSE: {
		if (!bFileSaved) {
			if (GetForegroundWindow() != hWnd) {
				FLASHWINFO FlashWindowInfo = { sizeof(FlashWindowInfo), hWnd, FLASHW_TRAY | FLASHW_TIMER, 3 };
				FlashWindowEx(&FlashWindowInfo);
			}
			switch (MessageBoxW(hWnd, (wstring(UNSAVE_FILE_PROMPT) + szFileName + L'?').c_str(), L"Confirm", MB_YESNOCANCEL)) {
			case IDYES: {
				if (SendMessageW(hWnd, WM_COMMAND, IDA_SAVE, 0))
					return 0;
			}	break;
			case IDCANCEL: return 0;
			}
		}
	}	break;
	case WM_DESTROY: {
		DeleteBrush(hBrush_Background);
		PostQuitMessage(0);
	}	break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WndProc_PaintView(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static SIZE unitScroll, maxScroll;
	static HWND hWnd_Canvas;
	switch (uMsg) {
	case WM_CREATE: {
		WNDCLASSW wndClass = { 0 };
		wndClass.hInstance = ((LPCREATESTRUCTW)lParam)->hInstance;
		wndClass.lpszClassName = L"Canvas";
		wndClass.lpfnWndProc = WndProc_Canvas;
		wndClass.hCursor = LoadCursor(NULL, IDC_CROSS);
		wndClass.hbrBackground = SYS_WHITE_BRUSH;
		RegisterClassW(&wndClass);
		hWnd_Canvas = DPIAware_CreateWindowExW(iDPI, 0,
			wndClass.lpszClassName, NULL,
			WS_CHILD | WS_VISIBLE,
			CANVAS_LEFT, CANVAS_TOP, CANVAS_WIDTH + CANVAS_MARGIN + CANVAS_PADDING, CANVAS_HEIGHT + CANVAS_MARGIN + CANVAS_PADDING,
			hWnd, (HMENU)ID_CANVAS, wndClass.hInstance, NULL);
	}	break;
	case WM_SIZE: {
		static SIZE pageSize;
		if (lParam)
			pageSize = { LOWORD(lParam), HIWORD(lParam) };
		RECT rect;
		GetWindowRect(hWnd_Canvas, &rect);
		const SIZE fullSize = { rect.right - rect.left + Scale(CANVAS_LEFT, iDPI) + iActualMargin, rect.bottom - rect.top + Scale(CANVAS_TOP, iDPI) + iActualMargin };
		unitScroll = { max(pageSize.cx * pageSize.cx / fullSize.cx, 1), max(pageSize.cy * pageSize.cy / fullSize.cy, 1) };
		maxScroll = { max(fullSize.cx - pageSize.cx, 0), max(fullSize.cy - pageSize.cy, 0) };
		currentScroll = { min(currentScroll.cx, maxScroll.cx), min(currentScroll.cy, maxScroll.cy) };
		SCROLLINFO scrollInfo = { sizeof(scrollInfo) };
		scrollInfo.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		scrollInfo.nMax = fullSize.cx;
		scrollInfo.nPage = pageSize.cx;
		scrollInfo.nPos = currentScroll.cx;
		SetScrollInfo(hWnd, SB_HORZ, &scrollInfo, TRUE);
		scrollInfo.nMax = fullSize.cy;
		scrollInfo.nPage = pageSize.cy;
		scrollInfo.nPos = currentScroll.cy;
		SetScrollInfo(hWnd, SB_VERT, &scrollInfo, TRUE);
		SetWindowPos(hWnd_Canvas, NULL, Scale(CANVAS_LEFT, iDPI) - currentScroll.cx, Scale(CANVAS_TOP, iDPI) - currentScroll.cy, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	}	break;
	case WM_HSCROLL: case WM_VSCROLL: {
		const LONG lUnitScroll = (uMsg == WM_HSCROLL ? unitScroll.cx : unitScroll.cy),
			lMaxScroll = (uMsg == WM_HSCROLL ? maxScroll.cx : maxScroll.cy);
		auto& currentScrollPos = (uMsg == WM_HSCROLL ? currentScroll.cx : currentScroll.cy);
		auto pos = currentScrollPos;
		switch (LOWORD(wParam)) {
		case SB_LEFT /*SB_TOP*/: pos = 0; break;
		case SB_RIGHT /*SB_BOTTOM*/: pos = lMaxScroll; break;
		case SB_LINELEFT /*SB_LINEUP*/: pos -= lUnitScroll; break;
		case SB_LINERIGHT /*SB_LINEDOWN*/: pos += lUnitScroll; break;
		case SB_THUMBTRACK: pos = HIWORD(wParam); break;
		}
		pos = min(lMaxScroll, max(pos, 0));
		if (pos != currentScrollPos) {
			auto iDelta = currentScrollPos - pos;
			ScrollWindow(hWnd, (uMsg == WM_HSCROLL ? iDelta : 0), (uMsg == WM_HSCROLL ? 0 : iDelta), NULL, NULL);
			SCROLLINFO scrollInfo = { sizeof(scrollInfo) };
			scrollInfo.fMask = SIF_POS;
			scrollInfo.nPos = currentScrollPos = pos;;
			SetScrollInfo(hWnd, (uMsg == WM_HSCROLL ? SB_HORZ : SB_VERT), &scrollInfo, TRUE);
		}
	}	break;
	case WM_MOUSEWHEEL: {
		SCROLLBARINFO scrollBarInfo;
		scrollBarInfo.cbSize = sizeof(scrollBarInfo);
		if (GetScrollBarInfo(hWnd, OBJID_VSCROLL, &scrollBarInfo) &&
			!(scrollBarInfo.rgstate[0] & STATE_SYSTEM_INVISIBLE)) {
			PostMessageW(hWnd, WM_VSCROLL, GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
		}
		else goto horizontal_wheel;
	}	break;
	case WM_MOUSEHWHEEL: {
	horizontal_wheel:;
		SCROLLBARINFO scrollBarInfo;
		scrollBarInfo.cbSize = sizeof(scrollBarInfo);
		if (GetScrollBarInfo(hWnd, OBJID_HSCROLL, &scrollBarInfo) &&
			!(scrollBarInfo.rgstate[0] & STATE_SYSTEM_INVISIBLE))
			PostMessageW(hWnd, WM_HSCROLL, GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? SB_LINELEFT : SB_LINERIGHT, 0);
	}	break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WndProc_Canvas(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static const DWORD dwPixelSize = 3; // (BitmapInfo.bmiHeader.biBitCount = 24) / 8;
	static BOOL bLeftButtonDown;
	static LONG lParentWindowStyle;
	static DWORD dwScanLineSize, dwDIBSectionSize;
	static PBYTE pDIBSection;
	static auto_ptr<BYTE> previousDIBSection;
	static HPEN hPen;
	static HBITMAP hBitmap_Old;
	static COORD mouseCoord;
	static SIZE bitmapSize;
	static RECT canvasRect, rightShadowRect, bottomShadowRect, gripRect;
	switch (uMsg) {
	case WM_CREATE: {
		SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
		maxBitmapSize = { max(Scale(CANVAS_WIDTH,iDPI), GetSystemMetrics(SM_CXSCREEN)), max(Scale(CANVAS_HEIGHT,iDPI), GetSystemMetrics(SM_CYSCREEN)) };
		BITMAPINFO bitmapInfo = { sizeof(bitmapInfo.bmiHeader) };
		bitmapInfo.bmiHeader.biWidth = maxBitmapSize.cx;
		bitmapInfo.bmiHeader.biHeight = -maxBitmapSize.cy;
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 24;
		dwScanLineSize = (dwPixelSize * maxBitmapSize.cx + 3) & ~3;
		dwDIBSectionSize = maxBitmapSize.cy * dwScanLineSize;
		hDC_Canvas = GetDC(hWnd);
		hDC_Memory = CreateCompatibleDC(NULL);
		HBITMAP hBitmap = CreateDIBSection(NULL, &bitmapInfo, DIB_RGB_COLORS, (LPVOID*)&pDIBSection, NULL, 0);
		if (hBitmap == NULL) {
			DWORD dwLastError = GetLastError();
			MessageBoxW(hWnd, SysErrorMsg(dwLastError).GetMsg(), NULL, MB_OK | MB_ICONERROR);
			PostQuitMessage(dwLastError);
			break;
		}
		FillMemory(pDIBSection, dwDIBSectionSize, 0xff);
		hBitmap_Old = SelectBitmap(hDC_Memory, hBitmap);
	}	break;
	case WM_NCCALCSIZE: {
		if (wParam) {
			LPNCCALCSIZE_PARAMS lpParams = (LPNCCALCSIZE_PARAMS)lParam;
			lpParams->rgrc[0].bottom -= iActualMargin;
			lpParams->rgrc[0].right -= iActualMargin;
			return 0;
		}
	}
	case WM_NCHITTEST: {
		RECT rect;
		GetWindowRect(hWnd, &rect);
		if (GET_Y_LPARAM(lParam) >= rect.bottom - iActualMargin &&
			GET_X_LPARAM(lParam) >= rect.right - iActualMargin)
			return HTBOTTOMRIGHT;
	}	break;
	case WM_GETMINMAXINFO: {
		LPMINMAXINFO lpMinMaxInfo = (LPMINMAXINFO)lParam;
		lpMinMaxInfo->ptMinTrackSize = { 1 + iActualMargin, 1 + iActualMargin };
		lpMinMaxInfo->ptMaxTrackSize = { maxBitmapSize.cx + iActualMargin, maxBitmapSize.cy + iActualMargin };
		return 0;
	}
	case WM_ENTERSIZEMOVE: {
		HWND hWnd_Parent = GetParent(hWnd);
		lParentWindowStyle = GetWindowLongPtrW(hWnd_Parent, GWL_STYLE);
		SetWindowLongPtrW(hWnd_Parent, GWL_STYLE, lParentWindowStyle & ~WS_CLIPCHILDREN);
		canvasRect = { Scale(CANVAS_LEFT, iDPI), Scale(CANVAS_TOP, iDPI) };
		bitmapSize = canvasSize;
		previousDIBSection.reset(new BYTE[dwDIBSectionSize]);
		CopyMemory(previousDIBSection.get(), pDIBSection, dwDIBSectionSize);
	}	break;
	case WM_SIZING: {
		const PRECT pRect = (PRECT)lParam;
		HWND hWnd_Parent = GetParent(hWnd);
		HDC hDC = GetDC(hWnd_Parent);
		LockWindowUpdate(hWnd);
		DrawFocusRect(hDC, &canvasRect);
		canvasRect = { canvasRect.left - currentScroll.cx, canvasRect.top - currentScroll.cy, pRect->right - pRect->left - Scale(CANVAS_MARGIN, iDPI) - currentScroll.cx, pRect->bottom - pRect->top - Scale(CANVAS_MARGIN, iDPI) - currentScroll.cy };
		DrawFocusRect(hDC, &canvasRect);
		ReleaseDC(hWnd_Parent, hDC);
	}	break;
	case WM_SIZE: {
		canvasSize = { LOWORD(lParam), HIWORD(lParam) };
		rightShadowRect = { canvasSize.cx, Scale(CANVAS_SHADOW_OFFSET, iDPI), canvasSize.cx + Scale(CANVAS_SHADOW_LENGTH, iDPI), canvasSize.cy };
		bottomShadowRect = { Scale(CANVAS_SHADOW_OFFSET, iDPI), canvasSize.cy, canvasSize.cx, canvasSize.cy + Scale(CANVAS_SHADOW_LENGTH, iDPI) };
		gripRect = { canvasSize.cx, canvasSize.cy, canvasSize.cx + iActualMargin, canvasSize.cy + iActualMargin };
		HRGN hRgn = CreateRectRgn(0, 0, canvasSize.cx, canvasSize.cy),
			hRgn2 = CreateRectRgnIndirect(&rightShadowRect),
			hRgn3 = CreateRectRgnIndirect(&bottomShadowRect),
			hRgn4 = CreateRectRgnIndirect(&gripRect);
		CombineRgn(hRgn, hRgn, hRgn2, RGN_OR);
		CombineRgn(hRgn, hRgn, hRgn3, RGN_OR);
		CombineRgn(hRgn, hRgn, hRgn4, RGN_OR);
		SetWindowRgn(hWnd, hRgn, TRUE);
		DeleteRgn(hRgn4);
		DeleteRgn(hRgn3);
		DeleteRgn(hRgn2);
		DeleteRgn(hRgn);
		SendMessageW(hWnd_StatusBar, SB_SETTEXT, 1, (LPARAM)(L"Canvas Size: " + to_wstring(canvasSize.cx) + L" \xd7 " + to_wstring(canvasSize.cy) + L" px").c_str());
	}	break;
	case WM_EXITSIZEMOVE: {
		SetWindowLongPtrW(GetParent(hWnd), GWL_STYLE, lParentWindowStyle);
		if (canvasSize.cx != bitmapSize.cx || canvasSize.cy != bitmapSize.cy) {
			bFileSaved = FALSE;
			SendMessageW(GetParent(hWnd), WM_SIZE, 0, 0);
			PartialBitmap partialBitmap = { bitmapSize };
			LPCBYTE pPreviousDIBSection = previousDIBSection.get();
			if (canvasSize.cx < partialBitmap.Size.cx)
				for (long x = canvasSize.cx; x < partialBitmap.Size.cx; x++)
					for (long y = 0; y < canvasSize.cy; y++) {
						const DWORD i = x * dwPixelSize + y * dwScanLineSize;
						const RGBQUAD& rgbq = (RGBQUAD&)pPreviousDIBSection[i];
						if (rgbq.rgbRed != 0xff || rgbq.rgbGreen != 0xff || rgbq.rgbGreen != 0xff)
							partialBitmap.Pixels.push_back({ i, rgbq });
					}
			else if (canvasSize.cx > partialBitmap.Size.cx) {
				RECT rect = { partialBitmap.Size.cx, 0, canvasSize.cx, partialBitmap.Size.cy };
				FillRect(hDC_Memory, &rect, SYS_WHITE_BRUSH);
			}
			if (canvasSize.cy < partialBitmap.Size.cy)
				for (long x = 0; x < partialBitmap.Size.cx; x++)
					for (long y = canvasSize.cy; y < partialBitmap.Size.cy; y++) {
						const DWORD i = x * dwPixelSize + y * dwScanLineSize;
						const RGBQUAD& rgbq = (RGBQUAD&)pPreviousDIBSection[i];
						if (rgbq.rgbRed != 0xff || rgbq.rgbGreen != 0xff || rgbq.rgbGreen != 0xff)
							partialBitmap.Pixels.push_back({ i, rgbq });
					}
			else if (canvasSize.cy > partialBitmap.Size.cy) {
				RECT rect = { 0, partialBitmap.Size.cy, canvasSize.cx, canvasSize.cy };
				FillRect(hDC_Memory, &rect, SYS_WHITE_BRUSH);
			}
			undoHistory.push(partialBitmap);
			redoHistory = decltype(redoHistory)();
			previousDIBSection.reset(nullptr);
			EnableMenuItem(hMenu, IDM_REDO, MF_DISABLED);
			EnableMenuItem(hMenu, IDM_UNDO, MF_ENABLED);
		}
	}	break;
	case WM_LBUTTONDOWN: {
		bLeftButtonDown = TRUE;
		SetCapture(hWnd);
		if (paintingTool != PaintingTools::ColorPicker) {
			mouseCoord = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			bitmapSize = canvasSize;
			previousDIBSection.reset(new BYTE[dwDIBSectionSize]);
			CopyMemory(previousDIBSection.get(), pDIBSection, dwDIBSectionSize);
			switch (paintingTool) {
			case PaintingTools::Pen: case PaintingTools::Eraser: {
				hPen = CreatePen(PS_SOLID,
					paintingTool == PaintingTools::Pen ? iPenWidth : iEraserWidth,
					paintingTool == PaintingTools::Pen ? penColor : 0xffffff);
				SelectPen(hDC_Canvas, hPen);
				SelectPen(hDC_Memory, hPen);
				SetPixelV(hDC_Canvas, mouseCoord.X, mouseCoord.Y, paintingTool == PaintingTools::Pen ? penColor : 0xffffff);
				SetPixelV(hDC_Memory, mouseCoord.X, mouseCoord.Y, paintingTool == PaintingTools::Pen ? penColor : 0xffffff);
				MoveToEx(hDC_Canvas, mouseCoord.X, mouseCoord.Y, NULL);
				MoveToEx(hDC_Memory, mouseCoord.X, mouseCoord.Y, NULL);
				LineTo(hDC_Canvas, mouseCoord.X, mouseCoord.Y);
				LineTo(hDC_Memory, mouseCoord.X, mouseCoord.Y);
			}	break;
			case PaintingTools::Fill: {
				COLORREF color = GetPixel(hDC_Canvas, mouseCoord.X, mouseCoord.Y);
				HBRUSH hBrush = CreateSolidBrush(penColor);
				SelectBrush(hDC_Canvas, hBrush);
				SelectBrush(hDC_Memory, hBrush);
				ExtFloodFill(hDC_Canvas, mouseCoord.X, mouseCoord.Y, color, FLOODFILLSURFACE);
				ExtFloodFill(hDC_Memory, mouseCoord.X, mouseCoord.Y, color, FLOODFILLSURFACE);
				DeleteBrush(hBrush);
			}	break;
			}
		}
	}	break;
	case WM_MOUSEMOVE: {
		const COORD coord = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		SendMessageW(hWnd_StatusBar, SB_SETTEXT, 0, (LPARAM)(coord.X >= 0 && coord.X < canvasSize.cx && coord.Y >= 0 && coord.Y < canvasSize.cy ? (L"Mouse Coordinate: " + to_wstring(coord.X + 1) + L" \xd7 " + to_wstring(coord.Y + 1) + L" px").c_str() : NULL));
		TRACKMOUSEEVENT trackMouseEvent = { sizeof(trackMouseEvent), TME_LEAVE, hWnd };
		TrackMouseEvent(&trackMouseEvent);
		if (bLeftButtonDown) {
			mouseCoord = coord;
			switch (paintingTool) {
			case PaintingTools::Pen: case PaintingTools::Eraser: {
				LineTo(hDC_Canvas, mouseCoord.X, mouseCoord.Y);
				LineTo(hDC_Memory, mouseCoord.X, mouseCoord.Y);
			}	break;
			}
		}
	}	break;
	case WM_MOUSELEAVE: SendMessageW(hWnd_StatusBar, SB_SETTEXT, 0, 0); break;
	case WM_LBUTTONUP: ReleaseCapture(); break;
	case WM_CAPTURECHANGED: {
		if (bLeftButtonDown) {
			bLeftButtonDown = FALSE;
			switch (paintingTool) {
			case PaintingTools::Pen: case PaintingTools::Eraser: DeletePen(hPen); // no "break;"
			case PaintingTools::Fill: {
				bFileSaved = FALSE;
				PartialBitmap partialBitmap = { bitmapSize };
				LPCBYTE pPreviousDIBSection = previousDIBSection.get();
				for (long x = 0; x < partialBitmap.Size.cx; x++)
					for (long y = 0; y < partialBitmap.Size.cy; y++) {
						const DWORD i = x * dwPixelSize + y * dwScanLineSize;
						const RGBQUAD& rgbq = (RGBQUAD&)pDIBSection[i], & previousColor = (RGBQUAD&)pPreviousDIBSection[i];
						if (rgbq.rgbRed != previousColor.rgbRed || rgbq.rgbGreen != previousColor.rgbGreen || rgbq.rgbBlue != previousColor.rgbBlue)
							partialBitmap.Pixels.push_back({ i, previousColor });
					}
				undoHistory.push(partialBitmap);
				redoHistory = decltype(redoHistory)();
				previousDIBSection.reset(nullptr);
				EnableMenuItem(hMenu, IDM_REDO, MF_DISABLED);
				EnableMenuItem(hMenu, IDM_UNDO, MF_ENABLED);
			}	break;
			case PaintingTools::ColorPicker: {
				penColor = GetPixel(hDC_Canvas, LOWORD(lParam), HIWORD(lParam));
				switch (previousPaintingTool) {
				case PaintingTools::Pen: case PaintingTools::Eraser: paintingTool = previousPaintingTool; break;
				}
				CheckMenuRadioItem(hMenu, IDM_PEN, IDM_COLORPICKER, (UINT)paintingTool, MF_BYCOMMAND);
			}	break;
			}
		}
	}	break;
	case WM_COMMAND: {
		const WORD wParamLow = LOWORD(wParam);
		switch (wParamLow) {
		case IDA_UNDO: {
			if (!bLeftButtonDown && !undoHistory.empty()) {
				bFileSaved = FALSE;
				const auto& partialBitmap = undoHistory.top();
				if (canvasSize.cx < partialBitmap.Size.cx) {
					RECT rect = { canvasSize.cx, 0, partialBitmap.Size.cx, canvasSize.cy };
					FillRect(hDC_Memory, &rect, SYS_WHITE_BRUSH);
				}
				if (canvasSize.cy < partialBitmap.Size.cy) {
					RECT rect = { 0, canvasSize.cy, partialBitmap.Size.cx, partialBitmap.Size.cy };
					FillRect(hDC_Memory, &rect, SYS_WHITE_BRUSH);
				}
				vector<PIXEL> temp;
				for (const auto& pixel : partialBitmap.Pixels) {
					const DWORD i = pixel.dwDIBSectionIndex;
					RGBQUAD& rgbq = (RGBQUAD&)pDIBSection[i];
					temp.push_back({ i, { rgbq.rgbBlue, rgbq.rgbGreen, rgbq.rgbRed } });
					rgbq.rgbRed = pixel.Rgbq.rgbRed;
					rgbq.rgbGreen = pixel.Rgbq.rgbGreen;
					rgbq.rgbBlue = pixel.Rgbq.rgbBlue;
				}
				redoHistory.push({ canvasSize, temp });
				if (canvasSize.cx != partialBitmap.Size.cx || canvasSize.cy != partialBitmap.Size.cy) {
					SetWindowPos(hWnd, NULL, 0, 0, partialBitmap.Size.cx + iActualMargin, partialBitmap.Size.cy + iActualMargin, SWP_NOMOVE | SWP_NOZORDER);
					SendMessageW(GetParent(hWnd), WM_SIZE, 0, 0);
				}
				undoHistory.pop();
				EnableMenuItem(hMenu, IDM_REDO, MF_ENABLED);
				if (undoHistory.empty())
					EnableMenuItem(hMenu, IDM_UNDO, MF_DISABLED);
				BitBlt(hDC_Canvas, 0, 0, canvasSize.cx, canvasSize.cy, hDC_Memory, 0, 0, SRCCOPY);
			}
		}	break;
		case IDA_REDO: {
			if (!bLeftButtonDown && !redoHistory.empty()) {
				bFileSaved = FALSE;
				const auto& partialBitmap = redoHistory.top();
				if (canvasSize.cx < partialBitmap.Size.cx) {
					RECT rect = { canvasSize.cx, 0, partialBitmap.Size.cx, canvasSize.cy };
					FillRect(hDC_Memory, &rect, SYS_WHITE_BRUSH);
				}
				if (canvasSize.cy < partialBitmap.Size.cy) {
					RECT rect = { 0, canvasSize.cy, partialBitmap.Size.cx, partialBitmap.Size.cy };
					FillRect(hDC_Memory, &rect, SYS_WHITE_BRUSH);
				}
				vector<PIXEL> temp;
				for (const auto& pixel : partialBitmap.Pixels) {
					const DWORD i = pixel.dwDIBSectionIndex;
					RGBQUAD& rgbq = (RGBQUAD&)pDIBSection[i];
					temp.push_back({ i, { rgbq.rgbBlue, rgbq.rgbGreen, rgbq.rgbRed } });
					rgbq.rgbRed = pixel.Rgbq.rgbRed;
					rgbq.rgbGreen = pixel.Rgbq.rgbGreen;
					rgbq.rgbBlue = pixel.Rgbq.rgbBlue;
				}
				undoHistory.push({ canvasSize, temp });
				if (canvasSize.cx != partialBitmap.Size.cx || canvasSize.cy != partialBitmap.Size.cy) {
					SetWindowPos(hWnd, NULL, 0, 0, partialBitmap.Size.cx + iActualMargin, partialBitmap.Size.cy + iActualMargin, SWP_NOMOVE | SWP_NOZORDER);
					SendMessageW(GetParent(hWnd), WM_SIZE, 0, 0);
				}
				redoHistory.pop();
				EnableMenuItem(hMenu, IDM_UNDO, MF_ENABLED);
				if (redoHistory.empty())
					EnableMenuItem(hMenu, IDM_REDO, MF_DISABLED);
				BitBlt(hDC_Canvas, 0, 0, canvasSize.cx, canvasSize.cy, hDC_Memory, 0, 0, SRCCOPY);
			}
		}	break;
		case IDA_CANCEL: {
			if (bLeftButtonDown) {
				bLeftButtonDown = FALSE;
				ReleaseCapture();
				switch (paintingTool) {
				case PaintingTools::Pen: case PaintingTools::Eraser: DeletePen(hPen); // no "break;"
				case PaintingTools::Fill: {
					CopyMemory(pDIBSection, previousDIBSection.get(), dwDIBSectionSize);
					previousDIBSection.reset(nullptr);
					BitBlt(hDC_Canvas, 0, 0, canvasSize.cx, canvasSize.cy, hDC_Memory, 0, 0, SRCCOPY);
				}	break;
				}
			}
		}	break;
		}
	}	break;
	case WM_NCPAINT: {
		HDC hDC = GetWindowDC(hWnd);
		TRIVERTEX triVertices[] = {
			{ rightShadowRect.left, rightShadowRect.top, 0xaa00, 0xb400, 0xbe00 },
			{ rightShadowRect.right, rightShadowRect.bottom, 0xd200, 0xdc00, 0xe600 } },
			triVertices2[] = {
			{ bottomShadowRect.left, bottomShadowRect.top, triVertices[0].Red, triVertices[0].Green, triVertices[0].Blue },
			{ bottomShadowRect.right, bottomShadowRect.bottom, triVertices[1].Red, triVertices[1].Green, triVertices[1].Blue } };
		GRADIENT_RECT gradientRect = { 0, 1 };
		GdiGradientFill(hDC, triVertices, _countof(triVertices), &gradientRect, 1, GRADIENT_FILL_RECT_H);
		GdiGradientFill(hDC, triVertices2, _countof(triVertices2), &gradientRect, 1, GRADIENT_FILL_RECT_V);
		FillRect(hDC, &gripRect, (HBRUSH)COLOR_WINDOW);
		HBRUSH hBrush = CreateSolidBrush(RGB(14, 151, 249));
		FrameRect(hDC, &gripRect, hBrush);
		DeleteBrush(hBrush);
		ReleaseDC(hWnd, hDC);
	}	break;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hDC = BeginPaint(hWnd, &ps);
		BitBlt(hDC_Canvas, 0, 0, canvasSize.cx, canvasSize.cy, hDC_Memory, 0, 0, SRCCOPY);
		EndPaint(hWnd, &ps);
	}	break;
	case WM_DESTROY: {
		DeleteBitmap(SelectBitmap(hDC_Memory, hBitmap_Old));
		DeleteDC(hDC_Memory);
		ReleaseDC(hWnd, hDC_Canvas);
	}	break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}