/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <windows.h>

#include "detours/detours.h"


static HANDLE devmap_dir_hnd = INVALID_HANDLE_VALUE;
static HANDLE m_drive_hnd = INVALID_HANDLE_VALUE;


static
BOOL (WINAPI *TrueCreateProcessA)(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                                  LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                  LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                  BOOL bInheritHandles, DWORD dwCreationFlags,
                                  LPVOID lpEnvironment,
                                  LPCSTR lpCurrentDirectory,
                                  LPSTARTUPINFOA lpStartupInfo,
                                  LPPROCESS_INFORMATION lpProcessInformation) = CreateProcessA;

static
BOOL (WINAPI *TrueCreateProcessW)(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                                  LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                  LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                  BOOL bInheritHandles, DWORD dwCreationFlags,
                                  LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
                                  LPSTARTUPINFOW lpStartupInfo,
                                  LPPROCESS_INFORMATION lpProcessInformation) = CreateProcessW;


static
void setup_process(LPPROCESS_INFORMATION procinfo, DWORD creation_flags)
{
	if (!(creation_flags & CREATE_SUSPENDED))
		ResumeThread(procinfo->hThread);
}


API_EXPORTED
BOOL WINAPI CreateProcA(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                        LPSECURITY_ATTRIBUTES lpProcessAttributes,
                        LPSECURITY_ATTRIBUTES lpThreadAttributes,
                        BOOL bInheritHandles, DWORD dwCreationFlags,
                        LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
                        LPSTARTUPINFOA lpStartupInfo,
                        LPPROCESS_INFORMATION lpProcessInformation)
{
	BOOL rv;

	rv = TrueCreateProcessA(lpApplicationName, lpCommandLine,
                                lpProcessAttributes, lpThreadAttributes,
                                bInheritHandles,
                                dwCreationFlags & CREATE_SUSPENDED,
                                lpEnvironment,
                                lpCurrentDirectory, lpStartupInfo,
                                lpProcessInformation);
	if (rv)
		setup_process(lpProcessInformation, dwCreationFlags);

	return rv;
}


API_EXPORTED
BOOL WINAPI CreateProcW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                        LPSECURITY_ATTRIBUTES lpProcessAttributes,
                        LPSECURITY_ATTRIBUTES lpThreadAttributes,
                        BOOL bInheritHandles, DWORD dwCreationFlags,
                        LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
                        LPSTARTUPINFOW lpStartupInfo,
                        LPPROCESS_INFORMATION lpProcessInformation)
{
	BOOL rv;

	rv = TrueCreateProcessW(lpApplicationName, lpCommandLine,
                                lpProcessAttributes, lpThreadAttributes,
                                bInheritHandles,
                                dwCreationFlags & CREATE_SUSPENDED,
                                lpEnvironment,
                                lpCurrentDirectory, lpStartupInfo,
                                lpProcessInformation);
	if (rv)
		setup_process(lpProcessInformation, dwCreationFlags);

	return rv;
}


BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
	(void)hinst;
	(void)reserved;

	if (DetourIsHelperProcess())
    		return TRUE;

	if (dwReason == DLL_PROCESS_ATTACH) {
		DetourRestoreAfterWith();
        	DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach((PVOID*)&TrueCreateProcessA, CreateProcA);
		DetourAttach((PVOID*)&TrueCreateProcessW, CreateProcW);
        	DetourTransactionCommit();
	} else if (dwReason == DLL_PROCESS_DETACH) {
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach((PVOID*)&TrueCreateProcessA, CreateProcA);
		DetourDetach((PVOID*)&TrueCreateProcessW, CreateProcW);
		DetourTransactionCommit();
	}
	return TRUE;
}


API_EXPORTED
int setup_prefix_mount(HANDLE devmap, HANDLE drive)
{
	devmap_dir_hnd = devmap;
	m_drive_hnd = drive;
	return 0;
}
