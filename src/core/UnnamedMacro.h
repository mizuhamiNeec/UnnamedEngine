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

/**
 * @brief メモリリークチェッカーを有効にするマクロ
 * @details プログラム開始時に呼び出すことでメモリリークの検出を有効化します
 */
#define LeakChecker \
do { \
_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); \
} while(0)

/**
 * @brief メモリリークを検出してユーザーに通知する関数
 */
void CheckMemoryLeaksAndNotify();

/**
 * @brief アサーション失敗時のログ出力とエラーダイアログ表示を行う関数
 * @param expr 失敗した式の文字列表現
 * @param file ファイル名
 * @param line 行番号
 * @param func 関数名
 * @return 常に0を返す
 */
int LogAssertionFailure(
	const char* expr,
	const char* file,
	int         line,
	const char* func
);

/**
 * @brief 例外発生時にミニダンプファイルを作成する関数
 * @param ep 例外ポインタ（nullptrの場合は現在の状態をダンプ）
 */
void WriteDump(EXCEPTION_POINTERS* ep);

#ifdef _DEBUG
#define UASSERT(expr) (!!(expr) || (LogAssertionFailure(#expr, __FILE__, __LINE__, __FUNCTION__), ExitProcess(1), 0))
#else
#define UASSERT(expr) (!!(expr) || (LogAssertionFailure(#expr, __FILE__, __LINE__, __FUNCTION__), WriteDump(nullptr), ExitProcess(1), 0))
#endif

// HRESULTが失敗したときにぶん投げるマクロ 
#define THROW(hr) if (FAILED(hr)) throw std::runtime_error(std::string("error. HRESULT: 0x") + std::to_string(static_cast<unsigned long>(hr)))
