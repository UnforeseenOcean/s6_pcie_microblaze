#include "stdafx.h"

// make crt functions inline
#pragma intrinsic(memcpy, strcpy, strcmp)

#define RtDbgMsg
//--------------------------------------------------------------------------------------
PVOID RuntimeGetSysInf(SYSTEM_INFORMATION_CLASS InfoClass)
{
    NTSTATUS ns = STATUS_SUCCESS;
    ULONG RetSize = 0, Size = 0x100;
    PVOID Info = NULL;

    while (true) 
    {    
        // allocate memory for data
        if ((Info = GExAllocatePool(NonPagedPool, Size)) == NULL) 
        {
            RtDbgMsg(__FILE__, __LINE__, "ExAllocatePool() fails\n");
            return NULL;
        }

        RetSize = 0;

        // query system information
        ns = (NTSTATUS)GZwQuerySystemInformation(InfoClass, Info, Size, &RetSize);
        if (ns == STATUS_INFO_LENGTH_MISMATCH)
        {       
            // buffer is too small
            GExFreePool(Info);
            Info = NULL;

            if (RetSize > 0)
            {
                // allocate more memory ad try again
                Size = RetSize + 0x100;
            }            
            else
            {
                break;
            }
        }
        else
        {
            break;    
        }
    }

    if (!NT_SUCCESS(ns))
    {
        // function fails
        RtDbgMsg(__FILE__, __LINE__, "ZwQuerySystemInformation() fails; status: 0x%.8x\n", ns);

        if (Info)
        {
            GExFreePool(Info);
        }

        return NULL;
    }

    return Info;
}
//--------------------------------------------------------------------------------------
PVOID RuntimeGetModuleBase(char *ModuleName)
{
    PVOID pModuleBase = NULL;
    UNICODE_STRING usCommonHalName_1, usCommonNtName_1;
    UNICODE_STRING usCommonHalName_2, usCommonNtName_2;

    GRtlInitUnicodeString(&usCommonHalName_1, L"hal.dll");
    GRtlInitUnicodeString(&usCommonNtName_1, L"ntoskrnl.exe");
    GRtlInitUnicodeString(&usCommonHalName_2, L"hal");
    GRtlInitUnicodeString(&usCommonNtName_2, L"ntoskrnl");

    #define HAL_NAMES_NUM 6
    wchar_t *wcHalNames[] = 
    {
        L"hal.dll",      // Non-ACPI PIC HAL 
        L"halacpi.dll",  // ACPI PIC HAL
        L"halapic.dll",  // Non-ACPI APIC UP HAL
        L"halmps.dll",   // Non-ACPI APIC MP HAL
        L"halaacpi.dll", // ACPI APIC UP HAL
        L"halmacpi.dll"  // ACPI APIC MP HAL
    };

    #define NT_NAMES_NUM 4
    wchar_t *wcNtNames[] = 
    {
        L"ntoskrnl.exe", // UP
        L"ntkrnlpa.exe", // UP PAE
        L"ntkrnlmp.exe", // MP
        L"ntkrpamp.exe"  // MP PAE
    };

    PRTL_PROCESS_MODULES Info = (PRTL_PROCESS_MODULES)RuntimeGetSysInf(SystemModuleInformation);
    if (Info)
    {
        ANSI_STRING asModuleName;
        UNICODE_STRING usModuleName;

        GRtlInitAnsiString(&asModuleName, ModuleName);

        NTSTATUS ns = (NTSTATUS)GRtlAnsiStringToUnicodeString(&usModuleName, &asModuleName, TRUE);
        if (NT_SUCCESS(ns))
        {
            for (ULONG i = 0; i < Info->NumberOfModules; i++)
            {
                ANSI_STRING asEnumModuleName;
                UNICODE_STRING usEnumModuleName;

                GRtlInitAnsiString(
                    &asEnumModuleName, 
                    (char *)Info->Modules[i].FullPathName + Info->Modules[i].OffsetToFileName
                );

                NTSTATUS ns = (NTSTATUS)GRtlAnsiStringToUnicodeString(&usEnumModuleName, &asEnumModuleName, TRUE);
                if (NT_SUCCESS(ns))
                {
                    if ((BOOLEAN)GRtlEqualUnicodeString(&usModuleName, &usCommonHalName_1, TRUE) ||
                        (BOOLEAN)GRtlEqualUnicodeString(&usModuleName, &usCommonHalName_2, TRUE))
                    {
                        // hal.dll passed as module name
                        for (int i_m = 0; i_m < HAL_NAMES_NUM; i_m++)
                        {
                            UNICODE_STRING usHalName;
                            GRtlInitUnicodeString(&usHalName, wcHalNames[i_m]);

                            // compare module name from list with known HAL module name
                            if ((BOOLEAN)GRtlEqualUnicodeString(&usEnumModuleName, &usHalName, TRUE))
                            {
                                pModuleBase = (PVOID)Info->Modules[i].ImageBase;
                                break;
                            }
                        }
                    }
                    else if ((BOOLEAN)GRtlEqualUnicodeString(&usModuleName, &usCommonNtName_1, TRUE) ||
                             (BOOLEAN)GRtlEqualUnicodeString(&usModuleName, &usCommonNtName_2, TRUE))
                    {
                        // ntoskrnl.exe passed as module name
                        for (int i_m = 0; i_m < NT_NAMES_NUM; i_m++)
                        {
                            UNICODE_STRING usNtName;
                            GRtlInitUnicodeString(&usNtName, wcNtNames[i_m]);

                            // compare module name from list with known kernel module name
                            if ((BOOLEAN)GRtlEqualUnicodeString(&usEnumModuleName, &usNtName, TRUE))
                            {
                                pModuleBase = (PVOID)Info->Modules[i].ImageBase;
                                break;
                            }
                        }
                    }
                    else if ((BOOLEAN)GRtlEqualUnicodeString(&usModuleName, &usEnumModuleName, TRUE))
                    {
                        pModuleBase = (PVOID)Info->Modules[i].ImageBase;
                    }

                    GRtlFreeUnicodeString(&usEnumModuleName);

                    if (pModuleBase)
                    {
                        // module is found
                        break;
                    }
                }                    
            }                     

            GRtlFreeUnicodeString(&usModuleName);
        }        

        GExFreePool(Info);
    }

    return pModuleBase;
}
//--------------------------------------------------------------------------------------
PVOID RuntimeGetProcAddress(PVOID Image, char *lpszFunctionName)
{
    PVOID Ret = NULL;

    PIMAGE_NT_HEADERS pHeaders = (PIMAGE_NT_HEADERS)
        ((PUCHAR)Image + ((PIMAGE_DOS_HEADER)Image)->e_lfanew);
    
    ULONG ExportsAddr = pHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    ULONG ExportsSize = pHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;    
    
    BOOLEAN bByOrdinal = FALSE;
    ULONG_PTR Ordinal = (ULONG_PTR)lpszFunctionName;

    if (Ordinal < RUNTIME_MAX_ORDINAL)
    {
        // lpszFunctionName param is ordinal
        bByOrdinal = TRUE;
    }

    if (ExportsAddr == 0)
    {
        RtDbgMsg(__FILE__, __LINE__, __FUNCTION__"() ERROR: Export directory is not found\n");
        return NULL;
    }

    PIMAGE_EXPORT_DIRECTORY pImageExportDirectory = (PIMAGE_EXPORT_DIRECTORY)RVATOVA(Image, ExportsAddr);
    ULONG ExportAddr = 0;

    if (pImageExportDirectory->AddressOfFunctions == 0)
    {
        RtDbgMsg(__FILE__, __LINE__, __FUNCTION__"() ERROR: Exports not found\n");
        return NULL;
    }

    PULONG AddrOfFunctions = (PULONG)RVATOVA(Image, pImageExportDirectory->AddressOfFunctions);

    if (bByOrdinal)
    {
        if (pImageExportDirectory->Base > Ordinal)
        {
            RtDbgMsg(__FILE__, __LINE__, __FUNCTION__"() ERROR: Invalid ordinal number\n");
            return NULL;
        }

        Ordinal -= pImageExportDirectory->Base;

        if (Ordinal > pImageExportDirectory->NumberOfFunctions - 1)
        {
            RtDbgMsg(__FILE__, __LINE__, __FUNCTION__"() ERROR: Invalid ordinal number\n");
            return NULL;
        }

        // get export address by ordinal 
        ExportAddr = AddrOfFunctions[Ordinal];
    }
    else if (pImageExportDirectory->AddressOfNames != 0 && pImageExportDirectory->AddressOfNameOrdinals != 0)
    {
        PSHORT AddrOfOrdinals = (PSHORT)RVATOVA(Image, pImageExportDirectory->AddressOfNameOrdinals);
        PULONG AddrOfNames = (PULONG)RVATOVA(Image, pImageExportDirectory->AddressOfNames);

        // enumerate export names
        for (ULONG i = 0; i < pImageExportDirectory->NumberOfNames; i++)
        {
            char *lpszName = (char *)RVATOVA(Image, AddrOfNames[i]);

            if (!strcmp(lpszName, lpszFunctionName))
            {
                // return export address
                ExportAddr = AddrOfFunctions[AddrOfOrdinals[i]];
                break;
            }
        }
    }
              
    if (ExportAddr == 0)
    {
        RtDbgMsg(__FILE__, __LINE__, __FUNCTION__"() ERROR: Export is not found\n");
        return NULL;
    }

    Ret = RVATOVA(Image, ExportAddr);

    // check for the forwarded export
    if (ExportAddr > ExportsAddr &&
        ExportAddr < ExportsAddr + ExportsSize)
    {                
        char szModule[IMPORT_MAX_STRING_SIZE], *lpszFunction = NULL;
        
        memset(szModule, 0, sizeof(szModule));
        strcpy(szModule, (char *)Ret);

        // parse forwarded export string
        for (ULONG i = 0; i < IMPORT_MAX_STRING_SIZE; i++)
        {
            if (szModule[i] == '.')
            {
                // get module and function name
                lpszFunction = szModule + i + 1;
                szModule[i] = '\0';                            
                break;   
            }
            else if (szModule[i] == '\0')
            {
                break;
            }
        }
                
        if (lpszFunction == NULL)
        {
            RtDbgMsg(__FILE__, __LINE__, __FUNCTION__"() ERROR: Invalid forwarded export\n");
            return NULL;
        }

        PVOID Module = RuntimeGetModuleBase(szModule);
        if (Module == NULL)
        {                                                        
            RtDbgMsg(__FILE__, __LINE__, __FUNCTION__"() ERROR: Unknown forwarded export module %s\n", szModule);
            return NULL;
        }                            
            
        return RuntimeGetProcAddress(Module, lpszFunction);
    }

    return Ret;
}
//--------------------------------------------------------------------------------------
BOOLEAN RuntimeProcessImports(PVOID Image)
{
    PIMAGE_NT_HEADERS pHeaders = (PIMAGE_NT_HEADERS)
        ((PUCHAR)Image + ((PIMAGE_DOS_HEADER)Image)->e_lfanew);

    if (pHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress)
    {
        PIMAGE_IMPORT_DESCRIPTOR pImport = (PIMAGE_IMPORT_DESCRIPTOR)RVATOVA(
            Image,
            pHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress
        );

        ULONG ImportSize = pHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;

        RtDbgMsg(
            __FILE__, __LINE__, 
            "IMAGE_DIRECTORY_ENTRY_IMPORT: "IFMT"; Size: %d\n", 
            pImport, ImportSize
        );

        while (pImport->Name != 0)
        {    
            // load import library
            char *lpszLibName = (char *)RVATOVA(Image, pImport->Name);
            PVOID LibAddr = RuntimeGetModuleBase(lpszLibName);
            if (LibAddr == NULL)
            {
                RtDbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Error while loading \"%s\"\n", lpszLibName);
                return FALSE;
            }            

            RtDbgMsg(__FILE__, __LINE__, "LIB "IFMT": \"%s\"\n", LibAddr, lpszLibName);

            // process thunks data
            PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)RVATOVA(Image, pImport->FirstThunk);
            while (pThunk->u1.Ordinal != 0)
            {
                if (pThunk->u1.Ordinal & 0xf0000000)
                {
                    RtDbgMsg(__FILE__, __LINE__, __FUNCTION__ "() ERROR: Imports by ordianl are not supported\n");
                    return FALSE;    
                }
                
                PIMAGE_IMPORT_BY_NAME pName = (PIMAGE_IMPORT_BY_NAME)RVATOVA(Image, pThunk->u1.AddressOfData);
                char *lpszFuncName = (char *)&pName->Name;
             
                // lookup function address by name
                PVOID FuncAddr = RuntimeGetProcAddress(LibAddr, lpszFuncName);
                if (FuncAddr == NULL)
                {
                    RtDbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Error while importing \"%s\"\n", lpszFuncName);
                    return FALSE;
                }                   

                RtDbgMsg(__FILE__, __LINE__, "PROC "IFMT": \"%s\"\n", FuncAddr, lpszFuncName);

                *(PVOID *)pThunk = FuncAddr;
                pThunk += 1;
            }

            pImport += 1;
        }
    }    
    else
    {
        RtDbgMsg(__FILE__, __LINE__, __FUNCTION__ "() WARNING: Import directory not found\n");
    }

    return TRUE;
}
//--------------------------------------------------------------------------------------
// EoF
