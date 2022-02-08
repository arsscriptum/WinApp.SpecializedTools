// A simple program whose sole job is to run custom CreateProcess() commands.
// Implemented as a single file compilation unit.

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#ifdef _MBCS
#undef _MBCS
#endif

#include <cstdio>
#include <cstdlib>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tchar.h>

//#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT
# define PRINT_OUT _tprintf
#else
# define PRINT_OUT 
#endif // DEBUG_OUTPUT

#ifndef ERROR_ELEVATION_REQUIRED
	#define ERROR_ELEVATION_REQUIRED   740
#endif

#ifndef INHERIT_PARENT_AFFINITY
	#define INHERIT_PARENT_AFFINITY    0x00010000L
#endif

#ifndef STARTF_TITLEISLINKNAME
	#define STARTF_TITLEISLINKNAME     0x00000800L
#endif

#ifndef STARTF_TITLEISAPPID
	#define STARTF_TITLEISAPPID        0x00001000L
#endif

#ifndef STARTF_PREVENTPINNING
	#define STARTF_PREVENTPINNING      0x00002000L
#endif

#ifdef SUBSYSTEM_WINDOWS
// If the caller is a console application and is waiting for this application to complete, then attach to the console.
void InitVerboseMode(void)
{
	if (::AttachConsole(ATTACH_PARENT_PROCESS))
	{
		if (::GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE)
		{
			freopen("CONOUT$", "w", stdout);
			setvbuf(stdout, NULL, _IONBF, 0);
		}

		if (::GetStdHandle(STD_ERROR_HANDLE) != INVALID_HANDLE_VALUE)
		{
			freopen("CONOUT$", "w", stderr);
			setvbuf(stderr, NULL, _IONBF, 0);
		}
	}
}
#endif

void DumpSyntax(TCHAR *currfile)
{
	PRINT_OUT(_T("Syntax:  %s [options] [arguments]\n\n"), currfile);
	PRINT_OUT(_T("Options:\n"));
}

bool GxNetworkStarted = false;
SOCKET ConnectSocketHandle(TCHAR *socketip, unsigned short socketport, char fd, TCHAR *token, unsigned short tokenlen)
{
	if (socketip == NULL || socketport == 0 || tokenlen > 512)  return INVALID_SOCKET;

	// Initialize Winsock.
	if (!GxNetworkStarted)
	{
		WSADATA WSAData;
		if (::WSAStartup(MAKEWORD(2, 2), &WSAData))  return INVALID_SOCKET;
		if (LOBYTE(WSAData.wVersion) != 2 || HIBYTE(WSAData.wVersion) != 2)  return INVALID_SOCKET;

		GxNetworkStarted = true;
	}

	// Determine IPv4 or IPv6.
	SOCKET s;
	if (_tcschr(socketip, _T(':')) != NULL)
	{
		struct sockaddr_in6 si = {0};

		si.sin6_family = AF_INET6;
		si.sin6_port = ::htons(socketport);
		if (::InetPton(AF_INET6, socketip, &si.sin6_addr) != 1)  return INVALID_SOCKET;

		s = ::WSASocket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
		if (s == INVALID_SOCKET)  return INVALID_SOCKET;

		if (::connect(s, (sockaddr *)&si, sizeof(si)) != 0)
		{
			::closesocket(s);

			return INVALID_SOCKET;
		}
	}
	else
	{
		struct sockaddr_in si = {0};

		si.sin_family = AF_INET;
		si.sin_port = ::htons(socketport);
		if (::InetPton(AF_INET, socketip, &si.sin_addr) != 1)  return INVALID_SOCKET;

		s = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
		if (s == INVALID_SOCKET)  return INVALID_SOCKET;

		if (::connect(s, (sockaddr *)&si, sizeof(si)) != 0)
		{
			::closesocket(s);

			return INVALID_SOCKET;
		}
	}

	// Do authentication first.  Send token to the target.
	if (tokenlen)
	{
		char tokenbuf[512];
		DWORD bytesread;

		// There's no particularly good way to handle errors here without completely rewriting the function.
		if (!::ReadFile(::GetStdHandle(STD_INPUT_HANDLE), tokenbuf, tokenlen, &bytesread, NULL) || (unsigned short)bytesread != tokenlen || ::send(s, tokenbuf, (int)tokenlen, 0) != tokenlen)
		{
			::closesocket(s);

			return INVALID_SOCKET;
		}
	}
	else if (token != NULL)
	{
		char tokenbuf[512];

		for (size_t x = 0; x < 512 && token[x]; x++)
		{
			if (token[x] <= 0xFF)  tokenbuf[tokenlen++] = (char)token[x];
		}

		if (!tokenlen || ::send(s, tokenbuf, (int)tokenlen, 0) != tokenlen)
		{
			::closesocket(s);

			return INVALID_SOCKET;
		}
	}

	// Send one byte of data to the target so that it knows which file descriptor this socket is associated with.
	if (::send(s, &fd, 1, 0) != 1)
	{
		::closesocket(s);

		return INVALID_SOCKET;
	}

	return s;
}

// Moved these out of the main function into the global namespace so the thread entry functions have relatively clean access to them.
SOCKET Gx_stdinsocket = INVALID_SOCKET;
HANDLE Gx_stdinwritehandle = NULL;

SOCKET Gx_stdoutsocket = INVALID_SOCKET;
HANDLE Gx_stdoutreadhandle = NULL;

SOCKET Gx_stderrsocket = INVALID_SOCKET;
HANDLE Gx_stderrreadhandle = NULL;

DWORD WINAPI StdinSocketHandler(LPVOID)
{
	char buffer[65536];
	int bufferlen;

	do
	{
		// Stdin socket -> stdin pipe.
		bufferlen = ::recv(Gx_stdinsocket, buffer, sizeof(buffer), 0);
		if (!bufferlen || bufferlen == SOCKET_ERROR)  break;

		if (!::WriteFile(Gx_stdinwritehandle, buffer, (DWORD)bufferlen, NULL, NULL))  break;

	} while (1);

	::closesocket(Gx_stdinsocket);
	::CloseHandle(Gx_stdinwritehandle);

	Gx_stdinsocket = INVALID_SOCKET;
	Gx_stdinwritehandle = NULL;

	return 0;
}

DWORD WINAPI StdoutSocketHandler(LPVOID)
{
	char buffer[65536];
	DWORD bufferlen;

	do
	{
		// Stdout pipe -> stdout socket.
		if (!::ReadFile(Gx_stdoutreadhandle, buffer, sizeof(buffer), &bufferlen, NULL))  break;

		if (bufferlen && ::send(Gx_stdoutsocket, buffer, (int)bufferlen, 0) != bufferlen)  break;

	} while (1);

	::closesocket(Gx_stdoutsocket);
	::CloseHandle(Gx_stdoutreadhandle);

	Gx_stdoutsocket = INVALID_SOCKET;
	Gx_stdoutreadhandle = NULL;

	return 0;
}

DWORD WINAPI StderrSocketHandler(LPVOID)
{
	char buffer[65536];
	DWORD bufferlen;

	do
	{
		// Stderr pipe -> stderr socket.
		if (!::ReadFile(Gx_stderrreadhandle, buffer, sizeof(buffer), &bufferlen, NULL))  break;

		if (bufferlen && ::send(Gx_stderrsocket, buffer, (int)bufferlen, 0) != bufferlen)  break;

	} while (1);

	::closesocket(Gx_stderrsocket);
	::CloseHandle(Gx_stderrreadhandle);

	Gx_stderrsocket = INVALID_SOCKET;
	Gx_stderrreadhandle = NULL;

	return 0;
}

int _tmain(int argc, TCHAR **argv)
{
	bool verbose = false;
	bool wait = false;
	bool terminate = false;
	DWORD waitamount = INFINITE;
	LPTSTR pidfile = NULL;
	DWORD priorityflag = 0;
#ifdef UNICODE
	DWORD createflags = CREATE_UNICODE_ENVIRONMENT;
#else
	DWORD createflags = 0;
#endif
	LPTSTR startdir = NULL;
	LPTSTR appname = NULL;
	STARTUPINFO startinfo = {0};
	SECURITY_ATTRIBUTES secattr = {0};
	TCHAR *stdinstr = _T(":stdin");
	TCHAR *stdoutstr = _T(":stdout");
	TCHAR *stderrstr = _T(":stderr");
	TCHAR *socketip = NULL;
	unsigned short socketport = 0;
	TCHAR *sockettoken = NULL;
	unsigned short sockettokenlen = 0;
	HANDLE temphandle = NULL, stdinread = NULL, stdoutwrite = NULL, stderrwrite = NULL;
	HANDLE stdinthread = NULL, stdoutthread = NULL, stderrthread = NULL;
	PROCESS_INFORMATION procinfo = {0};
	DWORD exitcode = 0;

	// Initialize structures.
	startinfo.cb = sizeof(startinfo);
	startinfo.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
	startinfo.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
	startinfo.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);
	startinfo.dwFlags |= STARTF_USESTDHANDLES;

	secattr.nLength = sizeof(secattr);
	secattr.bInheritHandle = TRUE;
	secattr.lpSecurityDescriptor = NULL;


	startinfo.dwFlags |= STARTF_USEFILLATTRIBUTE;
	startinfo.dwFlags |= STARTF_USESHOWWINDOW;
	
	createflags |= HIGH_PRIORITY_CLASS;
	createflags |= CREATE_NEW_CONSOLE;


	startinfo.dwFillAttribute |= FOREGROUND_RED;

	startinfo.dwFillAttribute |= FOREGROUND_GREEN;
	startinfo.dwFillAttribute |= FOREGROUND_INTENSITY;
	startinfo.wShowWindow = SW_SHOWNORMAL;


	TCHAR current_directory[MAX_PATH];
	TCHAR  system_directory[MAX_PATH];
	TCHAR  winsys_directory[MAX_PATH];
	TCHAR  windows_directory[MAX_PATH];

	GetCurrentDirectory(MAX_PATH,current_directory);
	GetSystemDirectory(system_directory,MAX_PATH);
	GetSystemWindowsDirectory(winsys_directory,MAX_PATH);
	GetWindowsDirectory(windows_directory,MAX_PATH);

	PRINT_OUT(_T("GetCurrentDirectory = %s\n"), current_directory);
	PRINT_OUT(_T("GetSystemDirectory = %s\n"), system_directory);
	PRINT_OUT(_T("GetSystemWindowsDirectory = %s\n"), winsys_directory);
	PRINT_OUT(_T("GetWindowsDirectory = %s\n"), windows_directory);

	
	TCHAR  commandline[MAX_PATH];
	TCHAR applicationName[MAX_PATH]; // 1 for null terminator, 1 for the slash
	UINT nCharactersWritten = GetWindowsDirectory(applicationName, MAX_PATH);
	_stprintf_s(applicationName, MAX_PATH, _T("%s\\%s"), windows_directory,TEXT("explorer.exe")); 
	
	if(argc == 2)
	{
		_stprintf_s(commandline, MAX_PATH, _T("%s %s"), applicationName, argv[1]);
	}
	else
	{
		_stprintf_s(commandline, MAX_PATH, _T("%s %s"), applicationName,current_directory); 
	}

	PRINT_OUT(_T("appname = %s\n"), applicationName);
	PRINT_OUT(_T("commandline = %s\n"), commandline);

	// Execute CreateProcess().
	if (!::CreateProcess(applicationName, commandline, &secattr, &secattr, (startinfo.dwFlags & STARTF_USESTDHANDLES ? TRUE : FALSE), createflags, NULL, startdir, &startinfo, &procinfo))
	{
		DWORD errnum = ::GetLastError();
		LPTSTR errmsg = NULL;

#ifdef SUBSYSTEM_WINDOWS
		InitVerboseMode();
#endif

		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errnum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errmsg, 0, NULL);
		PRINT_OUT(_T("An error occurred while attempting to start the process:\n"));
		if (errmsg == NULL)  PRINT_OUT(_T("%d - Unknown error\n\n"), errnum);
		else
		{
			PRINT_OUT(_T("%d - %s\n"), errnum, errmsg);
			::LocalFree(errmsg);
		}
		if (errnum == ERROR_ELEVATION_REQUIRED)  PRINT_OUT(_T("'%s' must be run as Administrator.\n"), appname);
		PRINT_OUT(_T("lpApplicationName = %s\n"), appname);
		PRINT_OUT(_T("lpCommandLine = %s\n"), commandline);

		exitcode = 1;
	}
	else
	{
		if (pidfile != NULL)
		{
			char pidbuffer[65];
			_itoa(procinfo.dwProcessId, pidbuffer, 10);
			HANDLE hpidfile = ::CreateFile(pidfile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &secattr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hpidfile == INVALID_HANDLE_VALUE)
			{
#ifdef SUBSYSTEM_WINDOWS
				InitVerboseMode();
#endif

				PRINT_OUT(_T("PID file '%s' was unable to be opened.\n"), pidfile);
			}
			else
			{
				::WriteFile(hpidfile, pidbuffer, (DWORD)strlen(pidbuffer), NULL, NULL);
				::CloseHandle(hpidfile);
			}
		}

		::CloseHandle(procinfo.hThread);
		if (wait)
		{
			// Wait for process to complete.
			if (verbose)
			{
				if (waitamount == INFINITE)  PRINT_OUT(_T("Waiting for process to complete...\n"));
				else  PRINT_OUT(_T("Waiting for process to complete (%ims)...\n"), waitamount);
			}

			// If socket handles are used, start relevant threads to pass the data around.
			if (Gx_stdinsocket != INVALID_SOCKET && stdinread != NULL)
			{
				::CloseHandle(stdinread);
				stdinread = NULL;

				stdinthread = ::CreateThread(NULL, 0, StdinSocketHandler, &startinfo, 0, NULL);

				if (stdinthread == NULL)
				{
#ifdef SUBSYSTEM_WINDOWS
					InitVerboseMode();
#endif

					PRINT_OUT(_T("The 'stdin' socket handler thread failed to start.\n"));
				}
			}

			if (Gx_stdoutsocket != INVALID_SOCKET && stdoutwrite != NULL)
			{
				::CloseHandle(stdoutwrite);
				stdoutwrite = NULL;

				stdoutthread = ::CreateThread(NULL, 0, StdoutSocketHandler, &startinfo, 0, NULL);

				if (stdoutthread == NULL)
				{
#ifdef SUBSYSTEM_WINDOWS
					InitVerboseMode();
#endif

					PRINT_OUT(_T("The 'stdout' socket handler thread failed to start.\n"));
				}
			}

			if (Gx_stderrsocket != INVALID_SOCKET && stderrwrite != NULL)
			{
				::CloseHandle(stderrwrite);
				stderrwrite = NULL;

				stderrthread = ::CreateThread(NULL, 0, StderrSocketHandler, &startinfo, 0, NULL);

				if (stderrthread == NULL)
				{
#ifdef SUBSYSTEM_WINDOWS
					InitVerboseMode();
#endif

					PRINT_OUT(_T("The 'stderr' socket handler thread failed to start.\n"));
				}
			}

			if (::WaitForSingleObject(procinfo.hProcess, waitamount) == WAIT_OBJECT_0)
			{
				if (!::GetExitCodeProcess(procinfo.hProcess, &exitcode))  exitcode = 0;
			}
			else if (terminate)
			{
				if (verbose)  PRINT_OUT(_T("Timed out.  Terminating.\n"));
				::TerminateProcess(procinfo.hProcess, 1);
				if (!::GetExitCodeProcess(procinfo.hProcess, &exitcode))  exitcode = 0;
			}
			else
			{
				// The child process is still running but this process should exit.  Get all threads to terminate.
				if (Gx_stdinsocket != INVALID_SOCKET)  ::closesocket(Gx_stdinsocket);
				if (Gx_stdoutsocket != INVALID_SOCKET)  ::closesocket(Gx_stdoutsocket);
				if (Gx_stderrsocket != INVALID_SOCKET)  ::closesocket(Gx_stderrsocket);

				Gx_stdinsocket = INVALID_SOCKET;
				Gx_stdoutsocket = INVALID_SOCKET;
				Gx_stderrsocket = INVALID_SOCKET;

				if (Gx_stdinwritehandle != NULL)  ::CloseHandle(Gx_stdinwritehandle);
				if (Gx_stdoutreadhandle != NULL)  ::CloseHandle(Gx_stdoutreadhandle);
				if (Gx_stderrreadhandle != NULL)  ::CloseHandle(Gx_stderrreadhandle);

				Gx_stdinwritehandle = NULL;
				Gx_stdoutreadhandle = NULL;
				Gx_stderrreadhandle = NULL;
			}
		}

		::CloseHandle(procinfo.hProcess);
	}

	if (GxNetworkStarted)
	{
		// Wait for the threads to finish.
		if (stdinthread != NULL && ::WaitForSingleObject(stdinthread, 0) != WAIT_OBJECT_0)  ::TerminateThread(stdinthread, INFINITE);
		if (stdoutthread != NULL)  ::WaitForSingleObject(stdoutthread, INFINITE);
		if (stderrthread != NULL)  ::WaitForSingleObject(stderrthread, INFINITE);

		if (Gx_stdinsocket != INVALID_SOCKET)  ::closesocket(Gx_stdinsocket);
		if (Gx_stdoutsocket != INVALID_SOCKET)  ::closesocket(Gx_stdoutsocket);
		if (Gx_stderrsocket != INVALID_SOCKET)  ::closesocket(Gx_stderrsocket);

		::WSACleanup();
	}

	// Let the OS clean up after this program.  It is lazy, but whatever.
	if (verbose)  PRINT_OUT(_T("Return code = %i\n"), (int)exitcode);

	return (int)exitcode;
}

#ifdef SUBSYSTEM_WINDOWS
#ifndef UNICODE
// Swiped from:  https://stackoverflow.com/questions/291424/canonical-way-to-parse-the-command-line-into-arguments-in-plain-c-windows-api
LPSTR* CommandLineToArgvA(LPSTR lpCmdLine, INT *pNumArgs)
{
	int retval;
	retval = ::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, NULL, 0);
	if (!SUCCEEDED(retval))  return NULL;

	LPWSTR lpWideCharStr = (LPWSTR)malloc(retval * sizeof(WCHAR));
	if (lpWideCharStr == NULL)  return NULL;

	retval = ::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, lpWideCharStr, retval);
	if (!SUCCEEDED(retval))
	{
		free(lpWideCharStr);

		return NULL;
	}

	int numArgs;
	LPWSTR* args;
	args = ::CommandLineToArgvW(lpWideCharStr, &numArgs);
	free(lpWideCharStr);
	if (args == NULL)  return NULL;

	int storage = numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; i++)
	{
		BOOL lpUsedDefaultChar = FALSE;
		retval = ::WideCharToMultiByte(CP_ACP, 0, args[i], -1, NULL, 0, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval))
		{
			::LocalFree(args);

			return NULL;
		}

		storage += retval;
	}

	LPSTR* result = (LPSTR *)::LocalAlloc(LMEM_FIXED, storage);
	if (result == NULL)
	{
		LocalFree(args);

		return NULL;
	}

	int bufLen = storage - numArgs * sizeof(LPSTR);
	LPSTR buffer = ((LPSTR)result) + numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++ i)
	{
		BOOL lpUsedDefaultChar = FALSE;
		retval = ::WideCharToMultiByte(CP_ACP, 0, args[i], -1, buffer, bufLen, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval))
		{
			::LocalFree(result);
			::LocalFree(args);

			return NULL;
		}

		result[i] = buffer;
		buffer += retval;
		bufLen -= retval;
	}

	::LocalFree(args);

	*pNumArgs = numArgs;
	return result;
}
#endif

int CALLBACK WinMain(HINSTANCE /* hInstance */, HINSTANCE /* hPrevInstance */, LPSTR lpCmdLine, int /* nCmdShow */)
{
	int argc;
	TCHAR **argv;
	int result;

#ifdef UNICODE
	argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
#else
	argv = CommandLineToArgvA(lpCmdLine, &argc);
#endif

	if (argv == NULL)  return 0;

	result = _tmain(argc, argv);

	::LocalFree(argv);

	return result;
}
#endif
