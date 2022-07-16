/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <windows.h>
#include <strsafe.h>

#include "detours/detours.h"

#include "internal-ntdll.h"
#include "mmpack-mount-prefix.h"
#include "mount-helper.h"


static const GUID mmpack_guid = {
	.Data1 = 0xadbc636a, 
	.Data2 = 0x353b,
	.Data3 = 0x4250,
	.Data4 = {0xa0, 0xc1, 0x93, 0x3d, 0x29, 0xfc, 0xd2, 0xb7},
};

struct devmap {
	HANDLE dir;
	HANDLE drive;
	DWORD size;
	char dll_path[];
};


static struct devmap* prefix_devmap;


/*****************************************************************************
 *                       Memory allocation without using CRT                 *
 *****************************************************************************/
static HANDLE heap_hnd = INVALID_HANDLE_VALUE;

static
void* mem_alloc(size_t size)
{
	if (heap_hnd == INVALID_HANDLE_VALUE)
		heap_hnd = GetProcessHeap();

	return HeapAlloc(heap_hnd, 0, size);
}


static
void mem_free(void* ptr)
{
	if (ptr)
		HeapFree(heap_hnd, 0, ptr);
}


/*****************************************************************************
 *                             NT path helpers                               *
 *****************************************************************************/

#define FILE_SHARE_ALL (FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE)

/**
 * get_nt_path() - return NT path on allocated string
 * @path:       path in UTF-16
 *
 * Return: allocated string holding the NT path. Free with mem_free().
 */
static
WCHAR* get_nt_path(const WCHAR* path)
{
	HANDLE hnd;
	DWORD len;
	WCHAR* ntpath = NULL;
	SECURITY_ATTRIBUTES sec_attrs = {
		.nLength = sizeof(SECURITY_ATTRIBUTES),
		.bInheritHandle = FALSE,
	};

	// Open the file to get the final path
	hnd = CreateFileW(path, READ_CONTROL, FILE_SHARE_ALL, &sec_attrs,
	                  OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (hnd == INVALID_HANDLE_VALUE)
		return NULL;

	// Get the len of path and allocate needed size
	if (!(len = GetFinalPathNameByHandleW(hnd, NULL, 0, VOLUME_NAME_NT))
	   || !(ntpath = mem_alloc((len+1) * sizeof(WCHAR))))
		goto exit;

	GetFinalPathNameByHandleW(hnd, ntpath, len+1, VOLUME_NAME_NT);

exit:
	CloseHandle(hnd);
	return ntpath;
}


/**
 * get_dll_path() - Get NT path of mapped dll
 * @dllhnd:     handle of library
 *
 * Return: allocated string holding the NT path. Free with mem_free().
 */
static
char* get_dll_path(HANDLE dllhnd)
{
	int rsz, maxlen;
	char* dllpath;

	// Get dll path as mapped in memory
	for (maxlen = 128;; maxlen *= 2) {
		dllpath = mem_alloc(maxlen*sizeof(*dllpath));
		if (!dllpath)
			return NULL;

		rsz = GetModuleFileNameA(dllhnd, dllpath, maxlen);
		if (rsz != maxlen)
			break;

		// If we reach here, the buffer is too small, let's continue
		mem_free(dllpath);
	}

	return dllpath;
}


/**
 * path_size() - Get size in bytes of char* path
 * @path:       null terminated path in char*
 *
 * Return: size in byte of path including the terminating nul.
 */
static
size_t path_size(const char* path)
{
	int len;

	// Get length of path (excluding terminating NUL)
	for (len = 0; path[len]; len++);

	return (len + 1) * sizeof(*path);
}



/*****************************************************************************
 *                       Device map manipulation                             *
 *****************************************************************************/
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
	HANDLE child_proc = procinfo->hProcess;
	struct devmap* child_dm;
	HANDLE child_hnds[2] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
	HANDLE curr_proc = GetCurrentProcess();
	const char* dlls[] = {prefix_devmap->dll_path};

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
	    || !WriteProcessMemory(child_proc, &child_dm->dir, child_hnds,
	                           sizeof(child_hnds), NULL))
		return -1;

	DetourUpdateProcessWithDll(child_proc, dlls, 1);

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
	size_t map_sz, dllpath_sz;
	char* dllpath;
	HMODULE dllhnd;
	int rv = -1;

	// Get handle of current dll and length of its pathname
	GetModuleHandleExW((GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			     | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT),
	                   (LPCWSTR)&prefix_devmap, &dllhnd);
	dllpath = get_dll_path(dllhnd);
	if (!dllpath)
		return -1;

	// Compute needed size for holding the devmap struct
	dllpath_sz = path_size(dllpath);
	map_sz = sizeof(*prefix_devmap) + dllpath_sz;

	// Allocate local structure devmap
	prefix_devmap = mem_alloc(map_sz);
	if (!prefix_devmap)
		goto exit;

	prefix_devmap->dir = dir;
	prefix_devmap->drive = drive;
	prefix_devmap->size = map_sz;
	CopyMemory(prefix_devmap->dll_path, dllpath, dllpath_sz);
	rv = 0;

exit:
	mem_free(dllpath);
	return rv;
}


static
int create_unique_devmap_dir(HANDLE* pdirhnd)
{
	UNICODE_STRING name;
	OBJECT_ATTRIBUTES attr;
	WCHAR devmap_path[128];
	DWORD pid, ts;
	FILETIME curr;

	// Generate a unique devmap path
	GetSystemTimeAsFileTime(&curr);
	ts = curr.dwLowDateTime;
	pid = GetCurrentProcessId();
	StringCbPrintfW(devmap_path, sizeof(devmap_path),
	                L"\\??\\mmpack-prefix-%08x-%08x", pid, ts);

	// Create the directory holding the prefix device map
	RtlInitUnicodeString(&name, devmap_path);
	InitializeObjectAttributes(&attr, &name,
	                           OBJ_CASE_INSENSITIVE, NULL, NULL);
	return NtCreateDirectoryObject(pdirhnd, DIRECTORY_ALL_ACCESS, &attr);
}


static
int prefix_mount_setup(WCHAR* prefix_path)
{
	HANDLE curr_proc = GetCurrentProcess();
	OBJECT_ATTRIBUTES attr;
	UNICODE_STRING name, target;
	HANDLE dir = INVALID_HANDLE_VALUE;
	HANDLE drive = INVALID_HANDLE_VALUE;
	WCHAR* nt_prefix_path;

	if (create_unique_devmap_dir(&dir))
		goto failure;

	// Transform the supplied prefix into an abspath relative to NT device
	// (insensitive to device map)
	nt_prefix_path = get_nt_path(prefix_path);
	if (!nt_prefix_path)
		return -1;

	// Create drive letter in devicemap
	RtlInitUnicodeString(&name, L"M:");
	InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE, dir, NULL);
	RtlInitUnicodeString(&target, nt_prefix_path);
	if (NtCreateSymbolicLinkObject(&drive, SYMBOLIC_LINK_ALL_ACCESS,
	                               &attr, &target)
	    || init_prefix_devmap(dir, drive))
		goto failure;

	mem_free(nt_prefix_path);
	return apply_devmap(curr_proc);

failure:
	if (drive != INVALID_HANDLE_VALUE)
		CloseHandle(drive);

	if (dir != INVALID_HANDLE_VALUE)
		CloseHandle(dir);

	mem_free(nt_prefix_path);
	return -1;
}


API_EXPORTED
const struct mount_mmpack_dispatch dispatch_table = {
	.setup = prefix_mount_setup,
};
