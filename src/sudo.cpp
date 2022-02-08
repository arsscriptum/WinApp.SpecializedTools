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


#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)
#define WARNING(desc) message(__FILE__ "(" STRINGIZE(__LINE__) ") : Warning: " #desc)


//#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT
#pragma WARNING (" -- Debug Output is enabled -- This is not the final build")
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

inline bool exist(const TCHAR *name)
{
    return GetFileAttributes(name) != INVALID_FILE_ATTRIBUTES;
}

#ifdef USE_HARDCODED_CREDS
	#pragma WARNING("******************** enter password before compile here ******************** ")
	#define ADMIN_USER  TEXT("Administrateur")
	#define ADMIN_PASS  TEXT("MaMemoireEstMaCle7955")
#endif

wchar_t *getEnvironmentValue(const wchar_t *varname)
{
	if(!_wgetenv(varname)){
		PRINT_OUT(_T("Error! Environment variable '%s' not defined!\n"), varname);
		return nullptr;

	}
	return _wgetenv(varname);
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
	
	#ifndef USE_HARDCODED_CREDS
	wchar_t* ADMIN_USER = getEnvironmentValue( _T("SUDO_USER") );
	wchar_t* ADMIN_PASS = getEnvironmentValue( _T("SUDO_PSWD") ); 
	if(!ADMIN_USER || !ADMIN_PASS){
		return -1;
	}
	#endif

	wchar_t* nirsoftPath = getEnvironmentValue( _T("NIRSOFT_ROOT") );
	wchar_t* nirsoftApp  = getEnvironmentValue( _T("NIRSOFT_APP") );

	if(!nirsoftPath || !nirsoftApp){
		return -1;
	}

	TCHAR  nirsoftCommandPath[MAX_PATH];
	_stprintf_s(nirsoftCommandPath, MAX_PATH, _T("%s\\%s"),nirsoftPath,nirsoftApp); 

	if(exist(nirsoftCommandPath) == false)
	{
		PRINT_OUT(_T("nirsoft command app at path = %s does not exists!\n"), nirsoftCommandPath);
	}


	wchar_t* DEFAULT_CMD = getEnvironmentValue( _T("ComSpec") );

	if(!DEFAULT_CMD){
		return -1;
	}
	
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
	
	memset(applicationName,0,sizeof(applicationName));
	memset(commandline,0,sizeof(commandline));

	//%SystemRoot%\system32\WindowsPowerShell\v1.0\powershell.exe
	if(argc == 2)
	{
		_stprintf_s(commandline, MAX_PATH, _T("%s runas %s %s %s"),nirsoftCommandPath,ADMIN_USER,ADMIN_PASS,argv[1]); 
	}
	else
	{
		_stprintf_s(commandline, MAX_PATH, _T("%s runas %s %s %s"),nirsoftCommandPath,ADMIN_USER,ADMIN_PASS,DEFAULT_CMD); 
	}

	PRINT_OUT(_T("appname = %s (nullptr)\n"), applicationName);
	PRINT_OUT(_T("commandline = %s\n"), commandline);

	// Execute CreateProcess().
	if (!::CreateProcess(nullptr, commandline, &secattr, &secattr, (startinfo.dwFlags & STARTF_USESTDHANDLES ? TRUE : FALSE), createflags, NULL, startdir, &startinfo, &procinfo))
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
			
		}

		::CloseHandle(procinfo.hProcess);
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
