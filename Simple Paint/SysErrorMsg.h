#pragma once

#include <Windows.h>

class SysErrorMsg {
private:
	LPWSTR lpwSystemErrorMessage = NULL;

public:
	SysErrorMsg(DWORD dwErrorCode) {
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dwErrorCode, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
			(LPWSTR)&lpwSystemErrorMessage,
			0,
			NULL);
	}

	LPCWSTR GetMsg() const { return lpwSystemErrorMessage; }

	~SysErrorMsg() { LocalFree(lpwSystemErrorMessage); }
};