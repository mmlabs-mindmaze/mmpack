/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <windows.h>

#include "detours/detours.h"

#define MAX_DLL_PATHNAME 32768
static WCHAR dll_pathname[MAX_DLL_PATHNAME];
static int dll_pathname_len;


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
void init_dll_pathname(HMODULE dllinst)
{
	int i;

	GetModuleFileNameW(dllinst, dll_pathname, MAX_DLL_PATHNAME);

	// Get length of DLL pathname
	for (i = 0; i < MAX_DLL_PATHNAME-1; i++) {
		if (dll_pathname[i] == L'\0') {
			dll_pathname_len = i;
			break;
		}
	}
}


static
void setup_process(PROCESS_INFORMATION* procinfo, DWORD creation_flags)
{
	FARPROC pfn;
	LPVOID arg;
	size_t argsz = (dll_pathname_len + 1) * sizeof(dll_pathname);
	HANDLE child_hnd = procinfo->hProcess;

	// Queue LoadLibrary(dll_pathname) in remote process
	arg = VirtualAllocEx(child_hnd, NULL, argsz, MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(child_hnd, arg, dll_pathname, argsz, NULL);
	pfn = GetProcAddress(GetModuleHandleW(L"kernel32"), "LoadLibraryW");
	QueueUserAPC(procinfo->hThread, pfn, (ULONG_PTR)arg);

	if (!(creation_flags & CREATE_SUSPENDED))
		ResumeThread(procinfo->hThread);
}


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
	(void)reserved;

	if (DetourIsHelperProcess())
    		return TRUE;

	if (dwReason == DLL_PROCESS_ATTACH) {
		init_dll_pathname(hinst);
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

