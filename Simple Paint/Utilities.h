#pragma once

#include <Windows.h>
#include <Shlwapi.h>
#include <CommCtrl.h>
#include <windowsx.h>
#include "SysErrorMsg.h"

#pragma comment(lib, "Shlwapi.lib")

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#pragma warning(disable:6287)
#pragma warning(disable:26495)
#pragma warning(disable:28252)

#pragma region Enable Win32 Visual Styles
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#ifdef _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#pragma endregion

#define SYS_WHITE_BRUSH (HBRUSH)(COLOR_WINDOW + 1)

#pragma region Support high DPI displays
#define Scale(iPixels, iDPI) MulDiv(iPixels, iDPI, USER_DEFAULT_SCREEN_DPI)
#define DPIAware_CreateWindowExW(iDPI, dwExStyle, lpClassName, lpWindowName, dwStyle, iX, iY, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam) CreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, Scale(iX, iDPI), Scale(iY, iDPI), Scale(nWidth, iDPI), Scale(nHeight, iDPI), hWndParent, hMenu, hInstance, lpParam)
#pragma endregion

BOOL SaveDCAs24bitBitmapFile(LPCWSTR lpcwFileName, HDC hDC, UINT uWidth, UINT uHeight)
{
	HANDLE hFile = CreateFileW(lpcwFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;
	DWORD dwLastError = ERROR_SUCCESS;
	BITMAPINFO bitmapInfo = { sizeof(bitmapInfo.bmiHeader) };
	bitmapInfo.bmiHeader.biWidth = uWidth;
	bitmapInfo.bmiHeader.biHeight = uHeight;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 24;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	DWORD dwDIBSectionSize = ((bitmapInfo.bmiHeader.biBitCount / 8 * uWidth + 3) & ~3) * uHeight;
	PBYTE pBits = NULL;
	HDC hDC_Memory = CreateCompatibleDC(NULL);
	HBITMAP hBitmap = CreateDIBSection(NULL, &bitmapInfo, DIB_RGB_COLORS, (LPVOID*)&pBits, NULL, 0);
	if (hBitmap == NULL)
		return FALSE;
	HBITMAP hBitmap_Old = SelectBitmap(hDC_Memory, hBitmap);
	BitBlt(hDC_Memory, 0, 0, uWidth, uHeight, hDC, 0, 0, SRCCOPY);
	BITMAPFILEHEADER bitmapFileHeader = { 0 };
	bitmapFileHeader.bfType = 0x4d42;
	bitmapFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bitmapFileHeader.bfSize = bitmapFileHeader.bfOffBits + dwDIBSectionSize;
	DWORD dwBytesWritten;
	WriteFile(hFile, &bitmapFileHeader, sizeof(bitmapFileHeader), &dwBytesWritten, NULL);
	WriteFile(hFile, &bitmapInfo.bmiHeader, sizeof(bitmapInfo.bmiHeader), &dwBytesWritten, NULL);
	if (!WriteFile(hFile, pBits, dwDIBSectionSize, &dwBytesWritten, NULL))
		dwLastError = GetLastError();
	CloseHandle(hFile);
	DeleteBitmap(SelectBitmap(hDC_Memory, hBitmap_Old));
	DeleteDC(hDC_Memory);
	if (dwLastError == ERROR_SUCCESS)
		return TRUE;
	DeleteFileW(lpcwFileName);
	SetLastError(dwLastError);
	return FALSE;
}