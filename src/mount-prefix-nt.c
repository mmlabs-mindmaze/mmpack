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
	while ( (c = *curr) != '\0') {
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

	// Read command line, read first argument (path of this program),
	// discard it, and keep the second argument (normally path of
	// prefix). After this, cmdline should point at the first argument
	// after prefix path
	cmdline = GetCommandLineW();
	cmdline = copy_first_arg(path, cmdline);
	cmdline = copy_first_arg(path, cmdline);

	// Create temporary drive M: pointing to prefix_path
	if (!DefineDosDeviceW(DDD_NO_BROADCAST_SYSTEM, MOUNT_TARGET, path)) {
		fprintf(stderr, "Failed to create dos device M: mapped"
		                " to %S: %lu\n", path, GetLastError());
		return EXIT_FAILURE;
	}

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
	if (!DefineDosDeviceW(DDD_NO_BROADCAST_SYSTEM|DDD_REMOVE_DEFINITION, MOUNT_TARGET, NULL))
		fprintf(stderr, "Warning: Failed to revert dos device: %lu\n", GetLastError());

	return exitcode;
}
