/*
Project Name: Simple Paint
Last Update: 2020/01/29

This project is hosted on https://github.com/Hydr10n/Simple-Paint
Copyright (C) Programmer-Yang_Xun@outlook.com. All Rights Reserved.
*/

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#include <memory>
#include <vector>
#include <stack>
#include <string>
#include "resource.h"
#include "Utilities.h"

#define APP_NAME L"Simple Paint"
#define WINDOW_TITLE_SUFFIX (L" - " APP_NAME)
#define DEFAULT_FILE_NAME L"Untitled"
#define UNSAVE_FILE_PROMPT L"Do you want to save changes to "
#define SAVE_FILE_FAIL_PROMPT L"Failed to save changes due to the following reason:\n"

#define CANVAS_LEFT 3
#define CANVAS_TOP 3
#define CANVAS_PADDING 3
#define CANVAS_MARGIN 10
#define PEN_WIDTH_1PX 1
#define PEN_WIDTH_2PX 2
#define PEN_WIDTH_4PX 4
#define PEN_WIDTH_8PX 8
#define	ERASER_WIDTH_1PX PEN_WIDTH_1PX
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

enum class PaintingTools { Pen = IDM_PEN, Eraser = IDM_ERASER, ColorPicker = IDM_COLORPICKER };

BOOL bFileSaved = TRUE;
int iDPI = USER_DEFAULT_SCREEN_DPI, iPenWidth = PEN_WIDTH_8PX, iEraserWidth = ERASER_WIDTH_8PX;
PaintingTools paintingTool = PaintingTools::Pen, previousPaintingTool = paintingTool;
COLORREF penColor = RGB(0, 99, 177);
SIZE maxBitmapSize, canvasSize;
HWND hWnd_StatusBar;
HMENU hMenu;
HDC hDC_Canvas, hDC_Memory;
HPEN hPen_Paint, hPen_Erase;
stack<PartialBitmap> undoHistory, redoHistory;

LRESULT CALLBACK WndProc_Main(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc_PaintView(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc_Canvas(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgProc_About(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	int iWndWidth = 1310, iWndHeight = 814;
	HDC hDC = GetDC(NULL);
	if (hDC) {
		SetProcessDPIAware();
		iDPI = GetDeviceCaps(hDC, LOGPIXELSX);
		iWndWidth = Scale(iWndWidth, iDPI);
		iWndHeight = Scale(iWndHeight, iDPI);
		ReleaseDC(NULL, hDC);
	}
	WNDCLASSW wndClass = { 0 };
	wndClass.hInstance = hInstance;
	wndClass.lpfnWndProc = WndProc_Main;
	wndClass.lpszClassName = L"SimplePaint";
	wndClass.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	RegisterClassW(&wndClass);
	HWND hWnd = CreateWindowW(wndClass.lpszClassName, (wstring(DEFAULT_FILE_NAME) + WINDOW_TITLE_SUFFIX).c_str(),
		WS_OVERLAPPEDWINDOW,
		(GetSystemMetrics(SM_CXSCREEN) - iWndWidth) / 2, (GetSystemMetrics(SM_CYSCREEN) - iWndHeight) / 2, iWndWidth, iWndHeight,
		NULL, NULL, hInstance, NULL);
	ShowWindow(hWnd, nShowCmd);
	UpdateWindow(hWnd);
	HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));
	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0))
		if (!TranslateAcceleratorW(hWnd, hAccel, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	DestroyAcceleratorTable(hAccel);
	return msg.wParam;
}

LRESULT CALLBACK WndProc_Main(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static BOOL bFileEverSaved;
	static int iStatusBarHeight;
	static WCHAR szFileName[MAX_PATH] = DEFAULT_FILE_NAME;
	static HWND hWnd_PaintView;
	static HBRUSH hBrush_Background;
	switch (uMsg) {
	case WM_CREATE: {
		const HINSTANCE hInstance = ((LPCREATESTRUCTW)lParam)->hInstance;
		const INT uParts[] = { Scale(200, iDPI), Scale(400, iDPI) };
		hWnd_StatusBar = CreateWindowW(STATUSCLASSNAMEW, NULL,
			WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
			hWnd, NULL, hInstance, NULL);
		SendMessageW(hWnd_StatusBar, SB_SETPARTS, _countof(uParts), (LPARAM)uParts);
		WNDCLASSW wndClass = { 0 };
		wndClass.lpszClassName = L"PaintView";
		wndClass.lpfnWndProc = WndProc_PaintView;
		wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		hBrush_Background = wndClass.hbrBackground = CreateSolidBrush(RGB(220, 230, 240));
		RegisterClassW(&wndClass);
		RECT statusBarRect;
		GetWindowRect(hWnd_StatusBar, &statusBarRect);
		iStatusBarHeight = statusBarRect.bottom - statusBarRect.top;
		hWnd_PaintView = CreateWindowW(wndClass.lpszClassName, NULL,
			WS_CHILD | WS_VISIBLE,
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
		SetWindowPos(hWnd_PaintView, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam) - iStatusBarHeight, SWP_NOZORDER);
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
				FillRect(hDC_Canvas, &rect, (HBRUSH)(COLOR_WINDOW + 1));
				FillRect(hDC_Memory, &rect, (HBRUSH)(COLOR_WINDOW + 1));
				undoHistory = decltype(undoHistory)();
				redoHistory = decltype(redoHistory)();
				EnableMenuItem(hMenu, IDM_UNDO, MF_DISABLED);
				EnableMenuItem(hMenu, IDM_REDO, MF_DISABLED);
			}
		} break;
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
			WCHAR szFileTitle[MAX_PATH];
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
		case IDM_PEN: case IDM_ERASER: case IDM_COLORPICKER: {
			CheckMenuRadioItem(hMenu, IDM_PEN, IDM_COLORPICKER, wParamLow, MF_BYCOMMAND);
			switch (wParamLow) {
			case IDM_COLORPICKER: paintingTool = PaintingTools::ColorPicker; break;
			default: {
				previousPaintingTool = paintingTool;
				paintingTool = (wParamLow == IDM_PEN ? PaintingTools::Pen : PaintingTools::Eraser);
				SelectObject(hDC_Canvas, wParamLow == IDM_PEN ? hPen_Paint : hPen_Erase);
				SelectObject(hDC_Memory, wParamLow == IDM_PEN ? hPen_Paint : hPen_Erase);
			}	break;
			}
		} break;
		case IDM_PENSIZE_1PX: iPenWidth = PEN_WIDTH_1PX; goto pensize_8px;
		case IDM_PENSIZE_2PX: iPenWidth = PEN_WIDTH_2PX; goto pensize_8px;
		case IDM_PENSIZE_4PX: iPenWidth = PEN_WIDTH_4PX; goto pensize_8px;
		case IDM_PENSIZE_8PX: iPenWidth = PEN_WIDTH_8PX; {
		pensize_8px:;
			CheckMenuRadioItem(hMenu, IDM_PENSIZE_1PX, IDM_PENSIZE_8PX, wParamLow, MF_BYCOMMAND);
			DeleteObject(hPen_Paint);
			hPen_Paint = CreatePen(PS_SOLID, iPenWidth, penColor);
			if (paintingTool == PaintingTools::Pen) {
				SelectObject(hDC_Canvas, hPen_Paint);
				SelectObject(hDC_Memory, hPen_Paint);
			}
		}	break;
		case IDM_ERASERSIZE_1PX: iEraserWidth = ERASER_WIDTH_1PX; goto erasersize_8px;
		case IDM_ERASERSIZE_2PX: iEraserWidth = ERASER_WIDTH_2PX; goto erasersize_8px;
		case IDM_ERASERSIZE_4PX: iEraserWidth = ERASER_WIDTH_4PX; goto erasersize_8px;
		case IDM_ERASERSIZE_8PX: iEraserWidth = ERASER_WIDTH_8PX; {
		erasersize_8px:;
			CheckMenuRadioItem(hMenu, IDM_ERASERSIZE_1PX, IDM_ERASERSIZE_8PX, wParamLow, MF_BYCOMMAND);
			DeleteObject(hPen_Erase);
			hPen_Erase = CreatePen(PS_SOLID, iEraserWidth, 0xffffff);
			if (paintingTool == PaintingTools::Eraser) {
				SelectObject(hDC_Canvas, hPen_Erase);
				SelectObject(hDC_Memory, hPen_Erase);
			}
		} break;
		case IDM_COLOR: {
			static COLORREF custColors[16] = { penColor };
			CHOOSECOLORW chooseColor = { sizeof(chooseColor) };
			chooseColor.hwndOwner = hWnd;
			chooseColor.lpCustColors = custColors;
			chooseColor.rgbResult = penColor;
			chooseColor.Flags = CC_FULLOPEN | CC_RGBINIT;
			if (ChooseColorW(&chooseColor)) {
				DeleteObject(hPen_Paint);
				penColor = chooseColor.rgbResult;
				hPen_Paint = CreatePen(PS_SOLID, iPenWidth, penColor);
				if (paintingTool == PaintingTools::Pen) {
					SelectObject(hDC_Canvas, hPen_Paint);
					SelectObject(hDC_Memory, hPen_Paint);
				}
			}
		}	break;
		case IDM_ABOUT: DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG_ABOUT), hWnd, DlgProc_About); break;
		default: PostMessageW(GetDlgItem(hWnd_PaintView, ID_CANVAS), uMsg, wParam, lParam); break;
		}
	}	break;
	case WM_CLOSE: {
		if (!bFileSaved)
			switch (MessageBoxW(hWnd, (wstring(UNSAVE_FILE_PROMPT) + szFileName + L'?').c_str(), L"Confirm", MB_YESNOCANCEL)) {
			case IDYES:
				if (SendMessageW(hWnd, WM_COMMAND, IDA_SAVE, 0))
					return 0;
				break;
			case IDCANCEL: return 0;
			}
	}	break;
	case WM_DESTROY: {
		DeleteObject(hBrush_Background);
		PostQuitMessage(0);
	}	break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WndProc_PaintView(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CREATE: {
		WNDCLASSW wndClass = { 0 };
		wndClass.lpszClassName = L"Canvas";
		wndClass.lpfnWndProc = WndProc_Canvas;
		wndClass.hCursor = LoadCursor(NULL, IDC_CROSS);
		wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		RegisterClassW(&wndClass);
		DPIAware_CreateWindowExW(iDPI, 0,
			wndClass.lpszClassName, NULL,
			WS_CHILD | WS_VISIBLE,
			CANVAS_LEFT, CANVAS_TOP, 1280 + CANVAS_MARGIN + CANVAS_PADDING, 720 + CANVAS_MARGIN + CANVAS_PADDING,
			hWnd, (HMENU)ID_CANVAS, ((LPCREATESTRUCTW)lParam)->hInstance, NULL);
	}	break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WndProc_Canvas(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static const DWORD dwPixelSize = 3; // (BitmapInfo.bmiHeader.biBitCount = 24) / 8;
	static BOOL bLeftButtonDown;
	static DWORD dwScanLineSize, dwDIBSectionSize;
	static PBYTE pDIBSection;
	static auto_ptr<BYTE> PreviousDIBSection;
	static HBITMAP hBitmap, hBitmap_Old;
	static COORD mouseCoord;
	static RECT canvasRect;
	static PartialBitmap partialBitmap;
	switch (uMsg) {
	case WM_CREATE: {
		maxBitmapSize = { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
		BITMAPINFO bitmapInfo = { sizeof(bitmapInfo.bmiHeader) };
		bitmapInfo.bmiHeader.biWidth = maxBitmapSize.cx;
		bitmapInfo.bmiHeader.biHeight = -maxBitmapSize.cy;
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 24;
		dwScanLineSize = (dwPixelSize * maxBitmapSize.cx + 3) & ~3;
		dwDIBSectionSize = maxBitmapSize.cy * dwScanLineSize;
		hDC_Canvas = GetDC(hWnd);
		hDC_Memory = CreateCompatibleDC(NULL);
		hBitmap = CreateDIBSection(NULL, &bitmapInfo, DIB_RGB_COLORS, (LPVOID*)&pDIBSection, NULL, 0);
		if (hBitmap == NULL) {
			DWORD dwLastError = GetLastError();
			MessageBoxW(hWnd, SysErrorMsg(dwLastError).GetMsg(), NULL, MB_OK | MB_ICONERROR);
			PostQuitMessage(dwLastError);
			break;
		}
		FillMemory(pDIBSection, dwDIBSectionSize, 0xff);
		hBitmap_Old = (HBITMAP)SelectObject(hDC_Memory, hBitmap);
		hPen_Paint = CreatePen(PS_SOLID, iPenWidth, penColor);
		hPen_Erase = CreatePen(PS_SOLID, iEraserWidth, 0xffffff);
		SelectObject(hDC_Canvas, hPen_Paint);
		SelectObject(hDC_Memory, hPen_Paint);
	}	break;
	case WM_NCCALCSIZE: {
		LPNCCALCSIZE_PARAMS lpParams = (LPNCCALCSIZE_PARAMS)lParam;
		lpParams->rgrc[0].bottom -= Scale(CANVAS_MARGIN + CANVAS_PADDING, iDPI);
		lpParams->rgrc[0].right -= Scale(CANVAS_MARGIN + CANVAS_PADDING, iDPI);
	}	return 0;
	case WM_NCHITTEST: {
		const int actualMargin = Scale(CANVAS_MARGIN + CANVAS_PADDING, iDPI);
		RECT rect;
		GetWindowRect(hWnd, &rect);
		if (GET_Y_LPARAM(lParam) >= rect.bottom - actualMargin &&
			GET_X_LPARAM(lParam) >= rect.right - actualMargin)
			return HTBOTTOMRIGHT;
		return HTCLIENT;
	}
	case WM_GETMINMAXINFO: ((LPMINMAXINFO)lParam)->ptMinTrackSize = { 1 + Scale(CANVAS_MARGIN + CANVAS_PADDING, iDPI), 1 + Scale(CANVAS_MARGIN + CANVAS_PADDING, iDPI) }; return 0;
	case WM_ENTERSIZEMOVE: {
		partialBitmap.Size = canvasSize;
		partialBitmap.Pixels = decltype(partialBitmap.Pixels)();
		PreviousDIBSection.reset(new BYTE[dwDIBSectionSize]);
		CopyMemory(PreviousDIBSection.get(), pDIBSection, dwDIBSectionSize);
	}	break;
	case WM_SIZING: {
		const PRECT pRect = (PRECT)lParam;
		HWND hWnd_Parent = GetParent(hWnd);
		HDC hDC = GetDC(hWnd_Parent);
		LockWindowUpdate(hWnd);
		DrawFocusRect(hDC, &canvasRect);
		canvasRect = { Scale(CANVAS_LEFT, iDPI), Scale(CANVAS_TOP, iDPI), pRect->right - pRect->left - Scale(CANVAS_MARGIN, iDPI), pRect->bottom - pRect->top - Scale(CANVAS_MARGIN, iDPI) };
		DrawFocusRect(hDC, &canvasRect);
		ReleaseDC(hWnd_Parent, hDC);
	}	break;
	case WM_SIZE: {
		canvasSize = { LOWORD(lParam), HIWORD(lParam) };
		SendMessageW(hWnd_StatusBar, SB_SETTEXT, 1, (LPARAM)(L"Canvas Size: " + to_wstring(canvasSize.cx) + L" \xd7 " + to_wstring(canvasSize.cy) + L" px").c_str());
	} break;
	case WM_EXITSIZEMOVE: {
		canvasRect = { 0 };
		if (canvasSize.cx != partialBitmap.Size.cx || canvasSize.cy != partialBitmap.Size.cy) {
			bFileSaved = FALSE;
			if (canvasSize.cx < partialBitmap.Size.cx || canvasSize.cy < partialBitmap.Size.cy) {
				LPCBYTE pPreviousDIBSection = PreviousDIBSection.get();
				for (long x = 0; x < partialBitmap.Size.cx; x++)
					for (long y = 0; y < partialBitmap.Size.cy; y++) {
						const DWORD i = x * dwPixelSize + y * dwScanLineSize;
						const RGBQUAD& rgbq = (RGBQUAD&)pPreviousDIBSection[i];
						if (rgbq.rgbRed != 0xff || rgbq.rgbGreen != 0xff || rgbq.rgbGreen != 0xff)
							partialBitmap.Pixels.push_back({ i, rgbq });
					}
				RECT rect = { canvasSize.cx, 0, partialBitmap.Size.cx, partialBitmap.Size.cy };
				FillRect(hDC_Canvas, &rect, (HBRUSH)(COLOR_WINDOW + 1));
				FillRect(hDC_Memory, &rect, (HBRUSH)(COLOR_WINDOW + 1));
				rect = { 0, canvasSize.cy, canvasSize.cx, partialBitmap.Size.cy };
				FillRect(hDC_Canvas, &rect, (HBRUSH)(COLOR_WINDOW + 1));
				FillRect(hDC_Memory, &rect, (HBRUSH)(COLOR_WINDOW + 1));
			}
			undoHistory.push(partialBitmap);
			redoHistory = decltype(redoHistory)();
			partialBitmap.Pixels = decltype(partialBitmap.Pixels)();
			PreviousDIBSection.reset(nullptr);
			EnableMenuItem(hMenu, IDM_REDO, MF_DISABLED);
			EnableMenuItem(hMenu, IDM_UNDO, MF_ENABLED);
		}
	}	break;
	case WM_LBUTTONDOWN: {
		bLeftButtonDown = TRUE;
		SetCapture(hWnd);
		switch (paintingTool) {
		case PaintingTools::Pen: case PaintingTools::Eraser: {
			mouseCoord = { (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };
			partialBitmap.Size = canvasSize;
			partialBitmap.Pixels = decltype(partialBitmap.Pixels)();
			PreviousDIBSection.reset(new BYTE[dwDIBSectionSize]);
			CopyMemory(PreviousDIBSection.get(), pDIBSection, dwDIBSectionSize);
			SetPixelV(hDC_Canvas, mouseCoord.X, mouseCoord.Y, paintingTool == PaintingTools::Pen ? penColor : 0xffffff);
			SetPixelV(hDC_Memory, mouseCoord.X, mouseCoord.Y, paintingTool == PaintingTools::Pen ? penColor : 0xffffff);
			MoveToEx(hDC_Canvas, mouseCoord.X, mouseCoord.Y, NULL);
			MoveToEx(hDC_Memory, mouseCoord.X, mouseCoord.Y, NULL);
			LineTo(hDC_Canvas, mouseCoord.X, mouseCoord.Y);
			LineTo(hDC_Memory, mouseCoord.X, mouseCoord.Y);
		} break;
		}
	}	break;
	case WM_MOUSEMOVE: {
		const COORD coord = { (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };
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
	case WM_MOUSELEAVE: SendMessageW(hWnd_StatusBar, SB_SETTEXT, 0, NULL); break;
	case WM_LBUTTONUP: {
		if (bLeftButtonDown) {
			bLeftButtonDown = FALSE;
			ReleaseCapture();
			switch (paintingTool) {
			case PaintingTools::Pen: case PaintingTools::Eraser: {
				bFileSaved = FALSE;
				RECT rect = { canvasSize.cx, 0, maxBitmapSize.cx, maxBitmapSize.cy };
				FillRect(hDC_Canvas, &rect, (HBRUSH)(COLOR_WINDOW + 1));
				FillRect(hDC_Memory, &rect, (HBRUSH)(COLOR_WINDOW + 1));
				rect = { 0, canvasSize.cy, canvasSize.cx, maxBitmapSize.cy };
				FillRect(hDC_Canvas, &rect, (HBRUSH)(COLOR_WINDOW + 1));
				FillRect(hDC_Memory, &rect, (HBRUSH)(COLOR_WINDOW + 1));
				LPCBYTE pPreviousDIBSection = PreviousDIBSection.get();
				for (long x = 0; x < partialBitmap.Size.cx; x++)
					for (long y = 0; y < partialBitmap.Size.cy; y++) {
						const DWORD i = x * dwPixelSize + y * dwScanLineSize;
						const RGBQUAD& rgbq = (RGBQUAD&)pDIBSection[i], & previousColor = (RGBQUAD&)pPreviousDIBSection[i];
						if (rgbq.rgbRed != previousColor.rgbRed || rgbq.rgbGreen != previousColor.rgbGreen || rgbq.rgbBlue != previousColor.rgbBlue)
							partialBitmap.Pixels.push_back({ i, previousColor });
					}
				undoHistory.push(partialBitmap);
				redoHistory = decltype(redoHistory)();
				partialBitmap.Pixels = decltype(partialBitmap.Pixels)();
				PreviousDIBSection.reset(nullptr);
				EnableMenuItem(hMenu, IDM_REDO, MF_DISABLED);
				EnableMenuItem(hMenu, IDM_UNDO, MF_ENABLED);
			}	break;
			case PaintingTools::ColorPicker: {
				penColor = GetPixel(hDC_Canvas, LOWORD(lParam), HIWORD(lParam));
				DeleteObject(hPen_Paint);
				hPen_Paint = CreatePen(PS_SOLID, iPenWidth, penColor);
				paintingTool = previousPaintingTool;
				switch (paintingTool) {
				case PaintingTools::Pen: case PaintingTools::Eraser: {
					SelectObject(hDC_Canvas, previousPaintingTool == PaintingTools::Pen ? hPen_Paint : hPen_Erase);
					SelectObject(hDC_Memory, previousPaintingTool == PaintingTools::Eraser ? hPen_Paint : hPen_Erase);
				}	break;
				}
				CheckMenuRadioItem(hMenu, IDM_PEN, IDM_COLORPICKER, (UINT)previousPaintingTool, MF_BYCOMMAND);
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
				const auto& pixels = partialBitmap.Pixels;
				vector<PIXEL> temp;
				for (const auto& pixel : pixels) {
					const DWORD i = pixel.dwDIBSectionIndex;
					RGBQUAD& rgbq = (RGBQUAD&)pDIBSection[i];
					temp.push_back({ i, { rgbq.rgbBlue, rgbq.rgbGreen, rgbq.rgbRed } });
					rgbq.rgbRed = pixel.Rgbq.rgbRed;
					rgbq.rgbGreen = pixel.Rgbq.rgbGreen;
					rgbq.rgbBlue = pixel.Rgbq.rgbBlue;
				}
				redoHistory.push({ canvasSize, temp });
				const int actualMargin = Scale(CANVAS_MARGIN + CANVAS_PADDING, iDPI);
				SetWindowPos(hWnd, NULL, 0, 0, partialBitmap.Size.cx + actualMargin, partialBitmap.Size.cy + actualMargin, SWP_NOMOVE | SWP_NOZORDER);
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
				const auto& pixels = partialBitmap.Pixels;
				vector<PIXEL> temp;
				for (const auto& pixel : pixels) {
					const DWORD i = pixel.dwDIBSectionIndex;
					RGBQUAD& rgbq = (RGBQUAD&)pDIBSection[i];
					temp.push_back({ i, { rgbq.rgbBlue, rgbq.rgbGreen, rgbq.rgbRed } });
					rgbq.rgbRed = pixel.Rgbq.rgbRed;
					rgbq.rgbGreen = pixel.Rgbq.rgbGreen;
					rgbq.rgbBlue = pixel.Rgbq.rgbBlue;
				}
				undoHistory.push({ canvasSize, temp });
				const int actualMargin = Scale(CANVAS_MARGIN + CANVAS_PADDING, iDPI);
				SetWindowPos(hWnd, NULL, 0, 0, partialBitmap.Size.cx + actualMargin, partialBitmap.Size.cy + actualMargin, SWP_NOMOVE | SWP_NOZORDER);
				redoHistory.pop();
				EnableMenuItem(hMenu, IDM_UNDO, MF_ENABLED);
				if (redoHistory.empty())
					EnableMenuItem(hMenu, IDM_REDO, MF_DISABLED);
				BitBlt(hDC_Canvas, 0, 0, canvasSize.cx, canvasSize.cy, hDC_Memory, 0, 0, SRCCOPY);
			}
		} break;
		case IDA_CANCEL: {
			if (bLeftButtonDown) {
				bLeftButtonDown = FALSE;
				ReleaseCapture();
				CopyMemory(pDIBSection, PreviousDIBSection.get(), dwDIBSectionSize);
				partialBitmap.Pixels = decltype(partialBitmap.Pixels)();
				PreviousDIBSection.reset(nullptr);
				BitBlt(hDC_Canvas, 0, 0, canvasSize.cx, canvasSize.cy, hDC_Memory, 0, 0, SRCCOPY);
			}
		}	break;
		}
	}	break;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hDC = BeginPaint(hWnd, &ps);
		BitBlt(hDC_Canvas, 0, 0, canvasSize.cx, canvasSize.cy, hDC_Memory, 0, 0, SRCCOPY);
		EndPaint(hWnd, &ps);
	} break;
	case WM_DESTROY: {
		ReleaseDC(hWnd, hDC_Canvas);
		DeleteObject(hBitmap);
		DeleteObject(hPen_Paint);
		DeleteObject(hPen_Erase);
		SelectObject(hDC_Memory, hBitmap_Old);
		DeleteDC(hDC_Memory);
	} break;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

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