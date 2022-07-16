/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <windows.h>

#include "detours/detours.h"

#include "internal-ntdll.h"
#include "mount-helper.h"


static const GUID mmpack_guid = {
	.Data1 = 0xadbc636a, 
	.Data2 = 0x353b,
	.Data3 = 0x4250,
	.Data4 = {0xa0, 0xc1, 0x93, 0x3d, 0x29, 0xfc, 0xd2, 0xb7},
};

struct devmap {
	LONG size;
	HANDLE dir;
	HANDLE drive;
	WCHAR dll_path[];
};


static struct devmap* prefix_devmap;


static
int apply_devmap(HANDLE target_proc)
{
	return NtSetInformationProcess(target_proc, ProcessDeviceMap,
	                               &prefix_devmap->dir,
	                               sizeof(prefix_devmap->dir));
}


LOCAL_SYMBOL
int prefix_mount_setup_child(PROCESS_INFORMATION* procinfo)
{
	FARPROC pfn;
	HANDLE child_proc = procinfo->hProcess;
	struct devmap* child_dm;
	HANDLE child_hnds[2] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
	HANDLE curr_proc = GetCurrentProcess();

	if (!prefix_devmap)
		return 0;

	if (apply_devmap(child_proc))
		return -1;

	// Copy prefix_devmap to child as payload
	child_dm = DetourCopyPayloadToProcessEx(child_proc,
	                                        &mmpack_guid,
                                                prefix_devmap,
	                                        prefix_devmap->size);
	if (!child_dm
	    || !DuplicateHandle(curr_proc, prefix_devmap->dir,
	                        child_proc, child_hnds,
	                        0, FALSE, DUPLICATE_SAME_ACCESS)
	    || !DuplicateHandle(curr_proc, prefix_devmap->drive,
	                        child_proc, child_hnds+1,
	                        0, FALSE, DUPLICATE_SAME_ACCESS)
	    || WriteProcessMemory(child_proc, &child_dm->dir, child_hnds,
	                          sizeof(child_hnds), NULL))
		return -1;

	// Queue LoadLibrary(dll_pathname) in remote process
	pfn = GetProcAddress(GetModuleHandleW(L"kernel32"), "LoadLibraryW");
	QueueUserAPC(procinfo->hThread, pfn, (ULONG_PTR)(child_dm->dll_path));

	return 0;
}


LOCAL_SYMBOL
void prefix_mount_process_startup(void)
{
	prefix_devmap = DetourFindPayloadEx(&mmpack_guid, NULL);
}


static
int init_prefix_devmap(HANDLE dir, HANDLE drive)
{
	LONG map_sz;
	DWORD pathlen;
	HMODULE dllhnd;

	// Get handle of current dll and length of its pathname
	GetModuleHandleExW((GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			     | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT),
	                   (LPCWSTR)&prefix_devmap, &dllhnd);

	// Compute needed size for holding the devmap struct
	pathlen = GetModuleFileNameW(dllhnd, NULL, 0);
	map_sz = sizeof(*prefix_devmap) + (pathlen + 1) * sizeof(WCHAR);
	
	// Allocate local structure devmap
	prefix_devmap = VirtualAlloc(NULL, map_sz, MEM_COMMIT, PAGE_READWRITE);
	if (!prefix_devmap)
		return -1;

	prefix_devmap->size = map_sz;
	prefix_devmap->dir = dir;
	prefix_devmap->drive = drive;
	GetModuleFileNameW(dllhnd, prefix_devmap->dll_path, pathlen+1);

	return 0;
}


API_EXPORTED
int prefix_mount_setup(WCHAR* prefix_path)
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
	                               &attr, &target)
	    || init_prefix_devmap(dir, drive))
		goto failure;

	return apply_devmap(curr_proc);

failure:
	if (drive != INVALID_HANDLE_VALUE)
		CloseHandle(drive);

	if (dir != INVALID_HANDLE_VALUE)
		CloseHandle(dir);

	return -1;
}
