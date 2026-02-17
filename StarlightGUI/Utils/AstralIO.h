#pragma once

#include "pch.h"

#define IOCTL_AX_ENUM_PROCESSES			    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x700, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AX_TERMINATE_PROCESS		    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AX_HIDE_PROCESS			    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AX_SET_PPL				    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AX_SET_TOKEN				    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AX_DEUTERIUM_INVOKE		    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
// We only use ExAllocatePool2() to allocate memory, memory itself is kernel memory, and caller (user-mode) can't access it. This is only used for some parameter deliveries.
#define IOCTL_AX_DEUTERIUM_ALLOCATE		    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AX_DEUTERIUM_FREE		        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
// We use ZwMapViewOfSection() to allocate memory, so the allocated memory is shared between kernel and user, and caller can directly access it.
#define IOCTL_AX_DEUTERIUM_ALLOCATE_MODERN	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_AX_DEUTERIUM_FREE_MODERN		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _PROCESS_INFO {
    ULONG Pid;
} PROCESS_INFO, * PPROCESS_INFO;

typedef struct _PROCESS_SET_PPL {
    ULONG Pid;
    UCHAR SignatureSigner;
} PROCESS_SET_PPL, * PPROCESS_SET_PPL;

typedef struct _PROCESS_SET_TOKEN {
    ULONG FromPid;
    ULONG ToPid;
} PROCESS_SET_TOKEN, * PPROCESS_SET_TOKEN;

typedef struct _PROCESS_DATA {
    ULONG64 Eprocess;
    ULONG Pid;
    ULONG ParentPid;
    ULONG64 VirtualSize;
    ULONG64 WorkingSetSize;
    ULONG64 WorkingSetPrivateSize;
    BOOLEAN IsWow64;
    WCHAR ImageName[256];
    WCHAR ImagePath[1024];
} PROCESS_DATA, * PPROCESS_DATA;

typedef struct _ENUM_PROCESS {
    PVOID Buffer;
    ULONG BufferSize;
    ULONG ProcessCount;
} ENUM_PROCESS, * PENUM_PROCESS;

typedef enum _DEUTERIUM_PROXY_VAR_TYPE {
    TYPE_EMPTY = 0,     // This param means the end of params. (eg. if Param3 is set to this value, it means there are only 2 params for this function.)
    TYPE_LONG = 1,
    TYPE_LONG64 = 2,
    TYPE_ULONG = 3,
    TYPE_ULONG64 = 4,
    TYPE_CHAR = 5,
    TYPE_UCHAR = 6,
    TYPE_WCHAR = 7,
    TYPE_SHORT = 8,
    TYPE_USHORT = 9,
    TYPE_BOOLEAN = 10,
    TYPE_PVOID = 11,    // This param is a pointer to memory address. (For PVOID and also some undefined structs and types.)
    TYPE_NULLABLE = 12, // This param is present but delivered as NULL.
    TYPE_VOID = 13      // Return as void. (For functions.)
} DEUTERIUM_PROXY_VAR_TYPE, * PDEUTERIUM_PROXY_VAR_TYPE;

typedef enum _DEUTERIUM_PROXY_INOUT_TYPE {
    TYPE_ALL = 0,   // Not mentioned.
    TYPE_IN = 1,    // _IN_
    TYPE_OUT = 2    // _OUT_
} DEUTERIUM_PROXY_INOUT_TYPE, * PDEUTERIUM_PROXY_INOUT_TYPE;

typedef enum _DEUTERIUM_PROXY_REFERENCE_TYPE {
    TYPE_NONE = 0,  // Value itself.
    TYPE_REF = 1,   // Reference.
    TYPE_DEREF = 2  // Dereference.
} DEUTERIUM_PROXY_REFERENCE_TYPE, * PDEUTERIUM_PROXY_REFERENCE_TYPE;

typedef struct _DEUTERIUM_MEMBER {
    DEUTERIUM_PROXY_VAR_TYPE MemberVarType;
    DEUTERIUM_PROXY_INOUT_TYPE InOutType;
    DEUTERIUM_PROXY_REFERENCE_TYPE ReferenceType;
    ULONG64 MemberAddress;
} DEUTERIUM_MEMBER, * PDEUTERIUM_MEMBER;

typedef struct _DEUTERIUM_FUNCTION {
    DEUTERIUM_PROXY_VAR_TYPE ReturnVarType;     // Set to TYPE_VOID meaning the function is returned as void.
    DEUTERIUM_PROXY_REFERENCE_TYPE ReferenceType;
    ULONG64 FunctionAddress;
    ULONG64 ReturnValueAddress;                 // The address where the return value should be written to. If the function returns void, this param is ignored.
} DEUTERIUM_FUNCTION, * PDEUTERIUM_FUNCTION;

// When a param is set to TYPE_EMPTY, it means the function params end here, so it should only be set at the last param, and when we detect it, stop parsing.
typedef struct _DEUTERIUM_PROXY_INVOKE {
    DEUTERIUM_FUNCTION Function;
    BOOLEAN ShouldAttach; // Whether to attach the caller process, for sharing some tables like HANDLEs.
    DEUTERIUM_MEMBER Param1;
    DEUTERIUM_MEMBER Param2;
    DEUTERIUM_MEMBER Param3;
    DEUTERIUM_MEMBER Param4;
    DEUTERIUM_MEMBER Param5;
    DEUTERIUM_MEMBER Param6;
    DEUTERIUM_MEMBER Param7;
    DEUTERIUM_MEMBER Param8;
    DEUTERIUM_MEMBER Param9;
    DEUTERIUM_MEMBER Param10;
} DEUTERIUM_PROXY_INVOKE, * PDEUTERIUM_PROXY_INVOKE;

typedef struct _DEUTERIUM_PROXY_ALLOCATE {
    ULONG Size;
    ULONG64 Address; // The address where the allocated memory should be. If using normal allocation, the address can only be delivered and operated by kernel. If using modern allocation, caller can directly access it.
} DEUTERIUM_PROXY_ALLOCATE, * PDEUTERIUM_PROXY_ALLOCATE;

typedef struct _DEUTERIUM_PROXY_FREE {
    ULONG64 Address;
} DEUTERIUM_PROXY_FREE, * PDEUTERIUM_PROXY_FREE;

typedef enum _PS_PROTECTED_SIGNER
{
    PsProtectedSignerNone = 0,
    PsProtectedSignerAuthenticode,
    PsProtectedSignerCodeGen,
    PsProtectedSignerAntimalware,
    PsProtectedSignerLsa,
    PsProtectedSignerWindows,
    PsProtectedSignerWinTcb,
    PsProtectedSignerWinSystem,
    PsProtectedSignerApp,
    PsProtectedSignerMax
} PS_PROTECTED_SIGNER, * PPS_PROTECTED_SIGNER;