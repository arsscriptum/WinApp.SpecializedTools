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

#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include <conio.h>

#pragma comment(lib, "psapi.lib") //remember added it

void PrintMemoryInfo( DWORD processID )
{
    HANDLE hProcess;
    PROCESS_MEMORY_COUNTERS pmc;

    printf( "\nProcess ID: %u\n", processID );

    // Print information about the memory usage of the process.
    hProcess = OpenProcess(  PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID );
	
    if (NULL == hProcess){
		return;
	}
  
	BOOL ret = GetProcessMemoryInfo( hProcess, &pmc, sizeof(pmc) );
    if (ret == TRUE)
    {
        printf( "\tWorkingSetSize: 0x%08X - %u\n",  pmc.WorkingSetSize, pmc.WorkingSetSize / 1024);        
        printf( "\tQuotaPeakPagedPoolUsage: 0x%08X - %u\n", pmc.QuotaPeakPagedPoolUsage ,  pmc.QuotaPeakPagedPoolUsage / 1024);
        printf( "\tQuotaPeakPagedPoolUsage: 0x%08X - %u\n", pmc.QuotaPeakPagedPoolUsage ,  pmc.QuotaPeakPagedPoolUsage / 1024);
        printf( "\tQuotaPagedPoolUsage: 0x%08X - %u\n", pmc.QuotaPagedPoolUsage, pmc.QuotaPagedPoolUsage / 1024);
        printf( "\tQuotaPeakNonPagedPoolUsage: 0x%08X - %u\n", pmc.QuotaPeakNonPagedPoolUsage,pmc.QuotaPeakNonPagedPoolUsage / 1024 );
        printf( "\tQuotaNonPagedPoolUsage: 0x%08X - %u\n",pmc.QuotaNonPagedPoolUsage , pmc.QuotaNonPagedPoolUsage / 1024);
        printf( "\tPagefileUsage: 0x%08X - %u\n", pmc.PagefileUsage, pmc.PagefileUsage  / 1024 ); 
        printf( "\tPeakPagefileUsage: 0x%08X - %u\n", pmc.PeakPagefileUsage, pmc.PeakPagefileUsage / 1024 );
        printf( "\tcb: 0x%08X - %u\n", pmc.cb , pmc.cb / 1024);     
    }

    CloseHandle( hProcess );
}

int _tmain(int argc, _TCHAR* argv[])
{
    // Get the list of process identifiers.
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;

    if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
        return 1;

    // Calculate how many process identifiers were returned.
    cProcesses = cbNeeded / sizeof(DWORD);

    // Print the memory usage for each process
    for ( i = 0; i < cProcesses; i++ )
        PrintMemoryInfo( aProcesses[i] );

    _getch();
     return 0;
}