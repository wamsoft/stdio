#include <windows.h>
#include <ncbind.hpp>

#include <stdio.h>
#include <iostream>
#include <string>


struct Stdio
{
	static bool isConsoleStdin;
	static bool isConsoleStdout;
	static bool isConsoleStderr;

	static int getState() {
		int state = 0;
		if (_fileno(stdin)  >= 0) state |= 0x01;
		if (_fileno(stdout) >= 0) state |= 0x02;
		if (_fileno(stderr) >= 0) state |= 0x04;
		return state;
	}

	static UINT consoleCP;
	static UINT consoleOutputCP;

	static void doneLocale() {
		if (consoleCP) ::SetConsoleCP(consoleCP);
		if (consoleOutputCP) ::SetConsoleOutputCP(consoleOutputCP);		
	}

	static void initLocale() {
		consoleCP = ::GetConsoleCP();
		consoleOutputCP = ::GetConsoleOutputCP();
		::atexit(doneLocale);
		::SetConsoleCP(CP_UTF8);
		::SetConsoleOutputCP(CP_UTF8);
		auto locale = std::locale(".UTF-8");
		std::wcin.imbue(locale);
		std::wcout.imbue(locale);
		std::wcerr.imbue(locale);
	}

	// コンソールと接続
	static tjs_error TJS_INTF_METHOD attach(tTJSVariant *result,
											tjs_int numparams,
											tTJSVariant **param,
											iTJSDispatch2 *objthis) {
		int state = numparams > 0 ? *param[0] : 0;
		bool ret = true;
		if (state == 0) {
			//VS2012以降でこの記述で正しく判定できない。ランタイムのバグと思われる
			//if (_fileno(stdin)  == -2) state |= 0x01;
			//if (_fileno(stdout) == -2) state |= 0x02;
			//if (_fileno(stderr) == -2) state |= 0x04;
			if (GetStdHandle(STD_INPUT_HANDLE) == 0) state |= 0x01;
			if (GetStdHandle(STD_OUTPUT_HANDLE) == 0) state |= 0x02;
			if (GetStdHandle(STD_ERROR_HANDLE) == 0) state |= 0x04;
		}
		// 接続先が無い場合はコンソールを開いてそこに接続する
		if (state != 0) {
			typedef BOOL (WINAPI* AttachConsoleFunc)(DWORD dwProcessId);
			HINSTANCE hDLL = LoadLibrary(L"kernel32.dll");
			AttachConsoleFunc AttachConsole = (AttachConsoleFunc)GetProcAddress(hDLL, "AttachConsole");
			if (AttachConsole && (*AttachConsole)(-1)) {
				if ((state & 0x01)) { freopen("CON", "r", stdin); isConsoleStdin = true; }
				if ((state & 0x02)) { freopen("CON", "w", stdout); isConsoleStdout = true; }
				if ((state & 0x04)) { freopen("CON", "w", stderr); isConsoleStderr = true; }
			} else {
				ret = false;
			}
			FreeLibrary(hDLL);
		}
		if (ret) {
			initLocale();
		}
		if (result) {
			*result = ret;
		}
		return TJS_S_OK;
	}

	// コンソールの割り当て
	static tjs_error TJS_INTF_METHOD alloc(tTJSVariant *result,
										   tjs_int numparams,
										   tTJSVariant **param,
										   iTJSDispatch2 *objthis) {
		int state = numparams > 0 ? *param[0] : 0;
		bool ret = true;
		if (state == 0) {
			if (_fileno(stdin)  == -2) state |= 0x01;
			if (_fileno(stdout) == -2) state |= 0x02;
			if (_fileno(stderr) == -2) state |= 0x04;
		}
		// 接続先が無い場合はコンソールを開いてそこに接続する
		if (state != 0) {
			if (::AllocConsole()) {
				if ((state & 0x01)) { freopen("CON", "r", stdin); isConsoleStdin = true; }
				if ((state & 0x02)) { freopen("CON", "w", stdout); isConsoleStdout = true; }
				if ((state & 0x04)) { freopen("CON", "w", stderr); isConsoleStderr = true; }
			} else {
				ret = false;
			}
		}
		if (ret) {
			initLocale();
		}
		if (result) {
			*result = ret;
		}
		return TJS_S_OK;
	}

	static tjs_error TJS_INTF_METHOD free(tTJSVariant *result,
										  tjs_int numparams,
										  tTJSVariant **param,
										  iTJSDispatch2 *objthis) {
		bool ret = ::FreeConsole() != 0;
		if (result) {
			*result = ret;
		}
		return TJS_S_OK;
	}
	
	// 標準入力からテキストを読み込む
	static tjs_error TJS_INTF_METHOD in(tTJSVariant *result,
										tjs_int numparams,
										tTJSVariant **param,
										iTJSDispatch2 *objthis) {
		if (result) {
			std::wstring str;
			if (isConsoleStdin) {
				// windows のバグ対策。なぜかコンソールだと stdin から Unicodeがよめない
				wchar_t buf[2048+1];
				DWORD readNum;
				::ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buf, 2048, &readNum, NULL);
				if (readNum > 0) {
					buf[readNum] = '\0';
					str = buf;
				}				 
			} else {
				std::getline(std::wcin, str);
			}
			*result = ttstr(str.c_str());
		}
		return TJS_S_OK;
	}

	// テキスト出力下請け関数
	static tjs_error TJS_INTF_METHOD _out(tjs_int numparams,
										  tTJSVariant **param,
										  std::wostream &wos,
										  HANDLE handle) {
		if (numparams > 0) {
			ttstr str = *param[0];
			if (handle) {
				DWORD writeNum;
				::WriteConsoleW(handle, str.c_str(), str.length(), &writeNum, NULL);
			} else {
				wos << str.c_str();
			}
		}
		return TJS_S_OK;
	}

	// 標準出力にテキストを出力
	static tjs_error TJS_INTF_METHOD out(tTJSVariant *result,
										 tjs_int numparams,
										 tTJSVariant **param,
										 iTJSDispatch2 *objthis) {
		return _out(numparams, param, std::wcout, isConsoleStdout ? GetStdHandle(STD_OUTPUT_HANDLE) : 0);
	}
	
	// 標準エラー出力にテキストを出力
	static tjs_error TJS_INTF_METHOD err(tTJSVariant *result,
										 tjs_int numparams,
										 tTJSVariant **param,
										 iTJSDispatch2 *objthis) {
		return _out(numparams, param, std::wcerr, isConsoleStderr ? GetStdHandle(STD_ERROR_HANDLE) : 0);
	}

	// 標準出力をフラッシュ
	static tjs_error TJS_INTF_METHOD flush(tTJSVariant *result,
										   tjs_int numparams,
										   tTJSVariant **param,
										   iTJSDispatch2 *objthis) {
		std::wcout << std::flush;
		return TJS_S_OK;
	}
};

bool Stdio::isConsoleStdin = false;
bool Stdio::isConsoleStdout = false;
bool Stdio::isConsoleStderr = false;

UINT Stdio::consoleCP = 0;
UINT Stdio::consoleOutputCP = 0;

NCB_ATTACH_CLASS(Stdio, System) {
	Property("stdioState", &Stdio::getState, 0);
	RawCallback("attachConsole",  &Stdio::attach, TJS_STATICMEMBER);
	RawCallback("allocConsole",  &Stdio::alloc, TJS_STATICMEMBER);
	RawCallback("freeConsole",  &Stdio::free, TJS_STATICMEMBER);
	RawCallback("stdin",  &Stdio::in, TJS_STATICMEMBER);
	RawCallback("stdout", &Stdio::out, TJS_STATICMEMBER);
	RawCallback("stderr", &Stdio::err, TJS_STATICMEMBER);
	RawCallback("flush", &Stdio::flush, TJS_STATICMEMBER);
}

void PreRegisterCallback()
{
}

NCB_PRE_REGIST_CALLBACK(PreRegisterCallback);
