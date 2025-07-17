#pragma once

#ifdef _WIN64
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <crtdbg.h>
#endif

#define LeakChecker \
do { \
_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); \
} while(0)

int LogAssertionFailure(
	const char* expr,
	const char* file,
	int         line,
	const char* func
);

void WriteDump(EXCEPTION_POINTERS* ep);

#pragma warning(push)
#pragma warning(disable:4127)
#define UASSERT(expr) \
(!!(expr) || LogAssertionFailure(#expr, __FILE__, __LINE__, __FUNCTION__))
#pragma warning(pop)

// HRESULTが失敗したときにぶん投げるマクロ 
#define THROW(hr) if (FAILED(hr)) throw std::runtime_error(std::string("error. HRESULT: 0x") + std::to_string(static_cast<unsigned long>(hr)))
