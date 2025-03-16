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
			Print(L"EFI Error: %r\n", Status);											\
            Print(L"Error in file: %a\n", __FILE__);                               		\
            Print(L"Error in line: %d\n", __LINE__);                                    \
            Print(L"Error in call: %a\n", #call);                                       \
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

    while (input[i] != '\0' && *wordCount < 9) {
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
        Print(L"SMBIOS Version: %u - %u\n", SmbiosTable->MajorVersion, SmbiosTable->MinorVersion);
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
    EFI_GUID Vendor_GUID = { 0x1BE4DF61, 0x93CA, 0x11d2, {0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C} };
    CHAR16 *BootNext = 0;
    UINTN BootMappingSize = 0;
    CHAR16 *BootMapping;
    
    // Get Wake-up Type to Boot Option mapping
    BootMapping = LibGetVariableAndSize(L"TestVar", &Vendor_GUID, &BootMappingSize);
    // BootMapping = L"null eins zwei drei vier fünf sechs sieben acht";
    if(!BootMapping) {
      Print(L"Could not get Boot Mapping\n");
      CALL(EFI_ABORTED);
    }
    
    CHAR16 *BootMappingWords[9];
    int wordCount = 0;
    
    SplitStringToWords(BootMapping, BootMappingWords, &wordCount);
    
    for (int i = 0; i < wordCount; i++) {
      Print(L"Wake-up Type 0x%02x : Bootfile %s\n", i, BootMappingWords[i]);
    }
    
    BootNext = BootMappingWords[WakeUpType];
    Print(L"BootNext: %s\n", BootNext);
    
    EFI_GUID DiskGuid = { 0x10cf4871, 0xc6e0, 0x4216, { 0xb9, 0xe0, 0xb5, 0xd4, 0x17, 0xe3, 0x40, 0x86 } };
    
    EFI_HANDLE *DiskHandles;
    UINTN HandleCount;
    EFI_DEVICE_PATH *DevicePath;

    CALL(LibLocateHandleByDiskSignature(MBR_TYPE_EFI_PARTITION_TABLE_HEADER, SIGNATURE_TYPE_GUID, &DiskGuid, &HandleCount, &DiskHandles));

    Print(L"Found %d EFI partitions\n", HandleCount);
    
    if (HandleCount != 1) {
      Print(L"Found %d EFI partitions, but expected 1\n", HandleCount);
      CALL(EFI_ABORTED);
    }
    
    if (!DiskHandles) {
      Print(L"Failed to locate EFI partition!\n");
      CALL(EFI_NOT_FOUND);
    }
    
    // Erstelle einen Gerätepfad aus dem Handle
    DevicePath = DevicePathFromHandle(DiskHandles[0]);
    if (!DevicePath) {
        Print(L"Failed to create device path!\n");
        CALL(EFI_NOT_FOUND);
    }

    // Konvertiere und drucke den Gerätepfad in eine UTF-16-String-Repräsentation
    Print(L"Device Path: %s\n", DevicePathToStr(DevicePath));

    EFI_DEVICE_PATH *FilePath = FileDevicePath(DiskHandles[0], BootNext);
    if (!FilePath) {
        Print(L"Unable to build file path.\n");
        CALL(EFI_NOT_FOUND);
    }

    Print(L"Loading: %s\n", DevicePathToStr(FilePath));
    
    EFI_HANDLE LoadedImageHandle;
    
    CALL(
        SystemTable->BootServices->LoadImage(
        FALSE,				// BootPolicy
        ImageHandle,		// ParentImageHandle
        FilePath,			// DevicePath
        NULL,				// SourceBuffer
        0,					// SourceSize
        &LoadedImageHandle  // ImageHandle
    ));
    
    CALL(
        SystemTable->BootServices->StartImage(
        LoadedImageHandle,
        NULL,
        NULL
    ));

    return EFI_SUCCESS;
}

