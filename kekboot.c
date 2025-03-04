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
            Print(L"Error in line: %d\n", __LINE__);                                    \
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

void SplitStringToWords(CHAR16 *input, CHAR16 **output, int *wordCount) {
    int i = 0;
    *wordCount = 0;

    while (input[i] != '\0') {
        // Überspringe führende Leerzeichen
        while (input[i] == L' ') {
            i++;
        }

        // Speicheradresse des Wortanfangs
        if (input[i] != '\0') {
            output[*wordCount] = &input[i];
            (*wordCount)++;
        }

        // Finde das Ende des Wortes
        while (input[i] != L' ' && input[i] != '\0') {
            i++;
        }

        // Ersetze das Ende des Worts mit Nullterminierung
        if (input[i] != '\0') {
            input[i] = '\0';
            i++;
        }
    }
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
    const EFI_STATUS StatusST = LibGetSystemConfigurationTable(&SMBIOS3TableGuid, (VOID**)&Smbios3Table);
    if (StatusST == EFI_SUCCESS) {
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
    //EFI_GUID GlobalVariable = EFI_GLOBAL_VARIABLE; // GUID for UEFI boot values
    CHAR16 BootNext = 0;
    //UINTN BootMappingSize = 0;
    CHAR16 *BootMapping;
    
    // Get Wake-up Type to Boot Option mapping
    //BootMapping = LibGetVariableAndSize(L"WakeUpType", &GlobalVariable, &BootMappingSize);
    BootMapping = L"null eins zwei drei vier fünf sechs sieben acht";
    if(!BootMapping) {
      Print(L"Could not get Boot Mapping\n");
      CALL(EFI_ABORTED);
    }
    
    CHAR16 *BootMappingWords[9];
    int wordCount = 0;
    
    SplitStringToWords(BootMapping, BootMappingWords, &wordCount);
    
    BootNext = *BootMappingWords[WakeUpType];
    
    EFI_HANDLE LoadedImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_DEVICE_PATH *DevicePath;
    
    // Get the loaded image protocol for the current image
    CALL(uefi_call_wrapper(
        SystemTable->BootServices->OpenProtocol,
        6,
        ImageHandle,
        &LoadedImageProtocol,
        (VOID **)&LoadedImage,
        ImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
    ));
    
    DevicePath = FileDevicePath(
        LoadedImage->DeviceHandle,
        &BootNext
    );
    
    if (!DevicePath) {
      Print(L"Could not get Device Path\n");
      CALL(EFI_ABORTED);
    }
    
    CALL(uefi_call_wrapper(
        SystemTable->BootServices->LoadImage,
        6,
        FALSE,
        ImageHandle,
        DevicePath,
        NULL,
        0,
        &LoadedImageHandle
    ));
    
    CALL(uefi_call_wrapper(
        SystemTable->BootServices->StartImage,
        3,
        LoadedImageHandle,
        NULL,
        NULL
    ));

    // Set BootNext variable
    /* CALL(ST->RuntimeServices->SetVariable(
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
            )); */

    return EFI_SUCCESS;
}

