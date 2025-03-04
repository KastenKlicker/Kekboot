#include <efi/efi.h>
#include <efi/efilib.h>
#include <efi/efiapi.h>

#include <efi/libsmbios.h>

/*
 *  This EFI applications chooses the boot entry depending on the wakeup type reported by SMBIOS.
 */

#define CALL(call)                                                                      \
    {                                                                                   \
        EFI_STATUS Status = call;                                                       \
        EFI_INPUT_KEY Key;                                                              \
        if (Status != EFI_SUCCESS)                                                      \
        {                                                                               \
            Print(L"EFI_ERROR: %r\n", Status);                                          \
            Print(L"Press any key to continue.");                                       \
            /* Now wait for a keystroke before continuing, otherwise your
            message will flash off the screen before you see it.

            First, we need to empty the console input buffer to flush
            out any keystrokes entered before this point */                             \
            Status = ST->ConIn->Reset(ST->ConIn, FALSE);                                \
            if (EFI_ERROR(Status))                                                      \
                return Status;                                                          \
            /* Now wait until a key becomes available.*/                                \
            EFI_EVENT WaitEvent = ST->ConIn->WaitForKey;                                \
            UINTN Index;                                                                \
            Status = ST->BootServices->WaitForEvent(1, &WaitEvent, &Index);             \
            if (!EFI_ERROR(Status))                                                     \
                ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);                              \
            return Status;                                                              \
        }                                                                               \
    }                                                                                    

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{

    InitializeLib(ImageHandle, SystemTable);
    
    SMBIOS_STRUCTURE_POINTER Smbios;
    SMBIOS_STRUCTURE_TABLE* SmbiosTable;
    SMBIOS3_STRUCTURE_TABLE* Smbios3Table;
    
    ST = SystemTable;

    // Remove all previous shown graphics
    CALL(ST->ConOut->ClearScreen(ST->ConOut));
    CALL(ST->ConOut->SetAttribute(ST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK));

    CALL(ST->ConOut->EnableCursor(ST->ConOut, TRUE));
    
    Print(L"KekBoot\n");

    /*
     *  Get WakeUpType
     *
     *  Try SMBIOS v3.x first and fall back to v1.x if not available
     */
    const EFI_STATUS smbiosTableStatus = LibGetSystemConfigurationTable(&SMBIOS3TableGuid, (VOID**)&Smbios3Table);
    if (smbiosTableStatus == EFI_SUCCESS) {
        Smbios.Hdr = (SMBIOS_HEADER*)Smbios3Table->TableAddress;
        // Print some useful information
        Print(L"SMBIOS Version: %u - %u - %u\n", Smbios3Table->MajorVersion, Smbios3Table->MinorVersion, Smbios3Table->DocRev);
        Print(L"SMBIOS Entry Point Revision: %u\n", Smbios3Table->EntryPointRevision);
    } else {
        CALL(LibGetSystemConfigurationTable(&SMBIOSTableGuid, (VOID**)&SmbiosTable));
        Print(L"Major SMBIOS Version: %u\n", SmbiosTable->MajorVersion);
        Smbios.Hdr = (SMBIOS_HEADER*)(UINTN)SmbiosTable->TableAddress;
    }

    UINT8 WakeUpType = 9; // Default value, not set by the UEFI specification

    // Get WakeUpType from the System Information table (Type 1) Offset 24 Byte
    while (Smbios.Hdr->Type < 127) {

        if (Smbios.Hdr->Type == 1) {
            Print(L"WakeUpType: 0x%02x\n", Smbios.Raw[24]);
            WakeUpType = Smbios.Raw[24];
            break;
        }

        LibGetSmbiosString(&Smbios, -1); // Go to the next table
    }

    // Couldn't find System Information table
    if (WakeUpType == 9) {
        Print(L"No valid SMBIOS WakeUpTpye entry found\n");
        CALL(EFI_NOT_FOUND);
    }

    /*
     * Set boot device based on WakeUpType
     */
    EFI_GUID GlobalVariable = EFI_GLOBAL_VARIABLE; // GUID for UEFI boot values
    UINT16 BootNext = 0;

    if (WakeUpType == 0x06) {
        BootNext = 0x0001; // Ubuntu
    } else {
        BootNext = 0x0000; // Windows
    }

    // Set BootNext variable
     CALL(ST->RuntimeServices->SetVariable(
        L"BootNext",
        &GlobalVariable,
        EFI_VARIABLE_NON_VOLATILE |
        EFI_VARIABLE_BOOTSERVICE_ACCESS |
        EFI_VARIABLE_RUNTIME_ACCESS,
        sizeof(BootNext),
        &BootNext   // new value
    ));

    Print(L"Set BootNext to 0x%04x\n", BootNext);

    // Reboot
    CALL(ST->RuntimeServices->ResetSystem(
                EfiResetCold,
                EFI_SUCCESS,
                0,
                NULL
            )); 

    return EFI_SUCCESS;
}

