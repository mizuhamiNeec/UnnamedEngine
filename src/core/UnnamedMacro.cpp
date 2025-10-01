#include <pch.h>
#include <cwchar>
#include <DbgHelp.h>

#pragma comment(lib, "DbgHelp.lib")

void CheckMemoryLeaksAndNotify() {
	if (_CrtDumpMemoryLeaks()) {
		MessageBoxW(
			nullptr,
			L"Memory leaks detected!\n\nCheck console output for details.",
			L"Unnamed LeakChecker",
			MB_OK | MB_ICONERROR
		);
	}
}

int LogAssertionFailure(
	const char* expr,
	const char* file,
	const int   line,
	const char* func
) {
	const std::string message =
		std::format(
			"Assertion failed: {}\nFile: {}\nLine: {}\nFunction: {}",
			expr,
			file,
			std::to_string(line),
			func
		);

	Fatal(
		"UASSERT",
		"Assertion failed: {}\nFile: {}\nLine: {}\nFunction: {}",
		expr,
		file,
		std::to_string(line),
		func
	);

	auto messageW = StrUtil::ToWString(message);

	int result;
#ifdef _DEBUG
	result = MessageBoxW(
		nullptr,
		messageW.c_str(),
		L"Engine Error",
		MB_ICONERROR | MB_ABORTRETRYIGNORE
	);
#else
	result = MessageBoxW(
		nullptr,
		messageW.c_str(),
		L"Engine Error",
		MB_ICONERROR | MB_OK
	);
#endif

	switch (result) {
	case IDABORT:
		// プロセス終了
		ExitProcess(1);
		break;
	case IDRETRY:
		// ブレーク
		__debugbreak();
		break;
	case IDIGNORE:
		break;
	default: ;
	}

	return 0;
}

void WriteDump(EXCEPTION_POINTERS* ep) {
	SYSTEMTIME st;
	GetLocalTime(&st);
	wchar_t filename[64];
	swprintf_s(filename, 64, L"crash_%04d-%02d-%02d_%02d-%02d-%02d.dmp",
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
		Fatal(
			"WriteDump",
			"Failed to create dump file: {}",
			std::to_string(GetLastError())
		);
	}
}
