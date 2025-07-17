#include <cwchar>
#include <windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")

//#include <Public/Core/Console/Log.h>

#include <utils/UnnamedMacro.h>

int LogAssertionFailure(
	const char* expr,
	const char* file,
	const int   line,
	const char* func
) {
	expr;
	file;
	line;
	func;
	// Core::Fatal(
	// 	"UnnamedAssert",
	// 	"Assertion failed: {}\n File: {}\n Line: {}\n Function: {}",
	// 	expr,
	// 	file,
	// 	std::to_string(line),
	// 	func
	// );

	MessageBoxW(
		nullptr,
		L"やあ",
		L"Engine Error",
		MB_ICONERROR | MB_OK
	);
	return 0;
}

void WriteDump(EXCEPTION_POINTERS* ep) {
	SYSTEMTIME st;
	GetLocalTime(&st);
	wchar_t filename[64];
	swprintf_s(filename, 64, L"crash_%04d%02d%02d_%02d%02d%02d.dmp",
	           st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	HANDLE hFile = CreateFileW(
		filename,
		GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
	);

	if (hFile != INVALID_HANDLE_VALUE) {
		MINIDUMP_EXCEPTION_INFORMATION mei = {};
		mei.ThreadId                       = GetCurrentThreadId();
		mei.ExceptionPointers              = ep;
		mei.ClientPointers                 = FALSE;

		MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			hFile,
			MiniDumpNormal,
			ep ? &mei : nullptr,
			nullptr,
			nullptr
		);

		CloseHandle(hFile);
	} else {
		// Core::Fatal(
		// 	"WriteDump",
		// 	"Failed to create dump file: {}",
		// 	std::to_string(GetLastError())
		// );
	}
}
