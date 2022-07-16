/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <windows.h>

#include "internal-ntdll.h"
#include "mount-helper.h"


struct devmap {
	HANDLE dir;
	HANDLE m_drive;
};


static struct devmap* prefix_devmap;


LOCAL_SYMBOL
int apply_devmap(HANDLE target_proc)
{
	return NtSetInformationProcess(target_proc, ProcessDeviceMap,
	                               &prefix_devmap->dir,
	                               sizeof(prefix_devmap->dir));
}


API_EXPORTED
int setup_prefix_mount(WCHAR* prefix_path)
{
	HANDLE curr_proc = GetCurrentProcess();
	OBJECT_ATTRIBUTES attr;
	UNICODE_STRING name, target;
	HANDLE dir = INVALID_HANDLE_VALUE;
	HANDLE drive = INVALID_HANDLE_VALUE;

	// Create the directory holding the prefix device map
	RtlInitUnicodeString(&name, L"\\??\\mmpack-prefix");
	InitializeObjectAttributes(&attr, &name,
	                           OBJ_CASE_INSENSITIVE, NULL, NULL);
	if (NtCreateDirectoryObject(&dir, DIRECTORY_ALL_ACCESS, &attr))
		goto failure;

	// Create drive letter in devicemap
	RtlInitUnicodeString(&name, L"M:");
	InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE, dir, NULL);
	RtlInitUnicodeString(&target, prefix_path);
	if (NtCreateSymbolicLinkObject(&drive, SYMBOLIC_LINK_ALL_ACCESS,
	                               &attr, &target))
		goto failure;

	// Allocate local structure devmap
	prefix_devmap = VirtualAlloc(NULL, sizeof(*prefix_devmap),
	                             MEM_COMMIT, PAGE_READWRITE);
	if (!prefix_devmap)
		goto failure;

	*prefix_devmap = (struct devmap) {.dir = dir, .m_drive = drive};

	return apply_devmap(curr_proc);

failure:
	if (drive != INVALID_HANDLE_VALUE)
		CloseHandle(drive);

	if (dir != INVALID_HANDLE_VALUE)
		CloseHandle(dir);

	return -1;
}
