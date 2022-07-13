/*
 * @mindmaze_header@
 */
#include <winternl.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define MOUNT_TARGET    L"M:"

// STYLE-EXCEPTION-BEGIN
typedef NTSTATUS (*_NtSetInformationProcess)(HANDLE ProcessHandle, PROCESS_INFORMATION_CLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength);
typedef VOID (*_RtlInitUnicodeString)(PUNICODE_STRING DestinationString, PCWSTR SourceString);
typedef NTSTATUS (*_NtCreateSymbolicLinkObject)(PHANDLE pHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PUNICODE_STRING DestinationName);
typedef NTSTATUS (*_NtCreateDirectoryObject)(PHANDLE DirectoryHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);
typedef NTSTATUS (*_NtOpenDirectoryObject)(PHANDLE DirectoryObjectHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);

static _RtlInitUnicodeString pRtlInitUnicodeString;
static _NtCreateDirectoryObject pNtCreateDirectoryObject;
static _NtSetInformationProcess pNtSetInformationProcess;
static _NtCreateSymbolicLinkObject pNtCreateSymbolicLinkObject;
static _NtOpenDirectoryObject pNtOpenDirectoryObject;

static
void load_nt_api(void)
{
	HMODULE ntmod = LoadLibraryW("ntdll.dll");

	pRtlInitUnicodeString = (_RtlInitUnicodeString)GetProcAddress(ntmod, "RtlInitUnicodeString");
	pNtCreateDirectoryObject = (_NtCreateDirectoryObject)GetProcAddress(ntmod, "NtCreateDirectoryObject");
	pNtSetInformationProcess = (_NtSetInformationProcess)GetProcAddress(ntmod, "NtSetInformationProcess");
	pNtCreateSymbolicLinkObject = (_NtCreateSymbolicLinkObject)GetProcAddress(ntmod, "NtCreateSymbolicLinkObject");
	pNtOpenDirectoryObject = (_NtOpenDirectoryObject)GetProcAddress(ntmod, "NtOpenDirectoryObject");

	if (!pRtlInitUnicodeString
	    || !pNtCreateDirectoryObject
	    || !pNtSetInformationProcess
	    || !pNtCreateSymbolicLinkObject
	    || !pNtOpenDirectoryObject) {
		fprintf(stderr, "Could not load all API's\n");
		exit(EXIT_FAILURE);
	}
}
// STYLE-EXCEPTION-END


/**************************************************************************
 *                                                                        *
 *                         Command line parsing                           *
 *                                                                        *
 **************************************************************************/
/**
 * write_nbackslash() - write a specified number of backslash in string
 * @str:        buffer to which the backslash must be written
 * @nbackslash: number of backslash character that must be written
 *
 * Return: the char pointer immediately after the @nbackslash have been
 * written, ie, @str + @nbackslash.
 */
static
WCHAR* write_nbackslash(WCHAR* str, int nbackslash)
{
	for (; nbackslash; nbackslash--)
		*str++ = '\\';

	return str;
}


/**
 * copy_first_arg() - copy first argument of a command line
 * @str:        pointer to a buffer that will receive the first argument
 * cmdline:     string of command line
 *
 * This function convert the first argument of the command line @cmdline and
 * copy to the buffer @str. The argument will be transformed following the
 * convention in:
 * https://docs.microsoft.com/en-us/windows/desktop/api/shellapi/nf-shellapi-commandlinetoargvw
 *
 * Return: the pointer in @cmdline buffer to next argument
 */
static
WCHAR* copy_first_arg(WCHAR* str, WCHAR* cmdline)
{
	WCHAR* curr = cmdline;
	int nbackslash = 0;
	int in_quotes = 0;
	WCHAR c;

	// Loop over all character in cmdline (until null termination)
	while ((c = *curr) != '\0') {
		switch (c) {
		case '\\':
			nbackslash++;
			break;

		case '"':
			if (nbackslash % 2 == 0) {
				str = write_nbackslash(str, nbackslash/2);
				in_quotes = !in_quotes;
			} else {
				str = write_nbackslash(str, (nbackslash-1)/2);
				*str++ = '"';
			}
			nbackslash = 0;
			break;

		case '\t':
		case ' ':
			// If not in quotes, terminate argument string,
			// otherwise normal processing
			if (!in_quotes)
				goto exit;

			/* fall through */

		default:
			str = write_nbackslash(str, nbackslash);
			nbackslash = 0;
			*str++ = c;
			break;
		}

		curr++;
	}
exit:
	*str = '\0';

	// Skip white space until next argument (or end of string)
	while (c == ' ' || c == '\t')
		c = *(++curr);

	return curr;
}


/**
 * check_running_mmpack() - check mmpack prefix input
 * @path: given input path
 *
 * This is used to prevent running two different prefixes at the same time
 *
 * This only is about prevent two distinct concurrent prefixes on the same
 * windows session. The same user logged twice independently (with password)
 * does not have that restriction.
 *
 * Returns: 0 on success (path can be used), or a non-zero value
 * otherwise
 */
static
int check_running_mmpack(WCHAR * path)
{
	DWORD rv;
	WCHAR prev_path[MAX_PATH];
	WCHAR * norm_prev_path;
	WCHAR norm_path[MAX_PATH];

	/* get and normalize the folder path of MOUNT_TARGET (M:) */
	rv = QueryDosDeviceW(MOUNT_TARGET, prev_path, MAX_PATH);
	if (rv == 0) {
		switch (GetLastError()) {
		case ERROR_FILE_NOT_FOUND:
			return 0;

		default:
			return -1;
		}
	}
	if (memcmp(prev_path, L"\\??\\", 4 * sizeof(WCHAR)) == 0)
		norm_prev_path = &prev_path[wcslen(L"\\??\\")];
	else
		norm_prev_path = &prev_path[0];

	/* normalize input path */
	GetFullPathNameW(path, MAX_PATH, norm_path, NULL);

	return wcscmp(norm_prev_path, norm_path);
}


/**************************************************************************
 *                                                                        *
 *                         mount prefix implementation                    *
 *                                                                        *
 **************************************************************************/

int main(void)
{
	PROCESS_INFORMATION proc_info;
	WCHAR path[MAX_PATH];
	WCHAR *cmdline;
	BOOL res;
	DWORD exitcode = EXIT_FAILURE;
	STARTUPINFOW si = {.cb = sizeof(si)};

	load_nt_api();

	// Read command line, read first argument (path of this program),
	// discard it, and keep the second argument (normally path of
	// prefix). After this, cmdline should point at the first argument
	// after prefix path
	cmdline = GetCommandLineW();
	cmdline = copy_first_arg(path, cmdline);
	cmdline = copy_first_arg(path, cmdline);

	// Start process with the same startup information but with the
	// stripped command line
	GetStartupInfoW(&si);
	copy_first_arg(path, cmdline);
	res = CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
	                     CREATE_SUSPENDED,
	                     NULL, NULL, &si, &proc_info);
	if (!res) {
		fprintf(stderr, "Cannot run %S : %lu\n", cmdline, GetLastError());
		goto exit;
	}

	// TODO: manipulation of child will take place here

	ResumeThread(proc_info.hThread);
	CloseHandle(proc_info.hThread);

	// Wait for termination of child in order to report its exit code
	WaitForSingleObject(proc_info.hProcess, INFINITE);
	GetExitCodeProcess(proc_info.hProcess, &exitcode);

exit:
	// Try remove temporary drive letter M:

	return exitcode;
}
