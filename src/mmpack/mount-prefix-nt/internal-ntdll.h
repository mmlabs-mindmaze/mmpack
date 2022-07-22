#ifndef INTERNAL_NTDLL_H
#define INTERNAL_NTDLL_H
#include <winternl.h>

#define SYMBOLIC_LINK_ALL_ACCESS 0xF0001
#define DIRECTORY_ALL_ACCESS 0xF000F

NTSYSAPI VOID RtlInitUnicodeString(PUNICODE_STRING DestinationString, PCWSTR SourceString);
NTSYSAPI NTSTATUS NtCreateSymbolicLinkObject(PHANDLE pHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PUNICODE_STRING DestinationName);
NTSYSAPI NTSTATUS NtCreateDirectoryObject(PHANDLE DirectoryHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);
NTSYSAPI NTSTATUS NtOpenDirectoryObject(PHANDLE DirectoryObjectHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);

#endif /* ifndef INTERNAL_NTDLL_H */
