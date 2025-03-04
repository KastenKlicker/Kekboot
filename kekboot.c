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

UINT64 FileSize(EFI_FILE_HANDLE FileHandle)
{
    UINT64 ret;
    EFI_FILE_INFO       *FileInfo;         /* file information structure */
    /* get the file's size */
    FileInfo = LibFileInfo(FileHandle);
    ret = FileInfo->FileSize;
    FreePool(FileInfo);
    return ret;
}

EFI_FILE_HANDLE GetVolume(EFI_HANDLE image)
{
    EFI_LOADED_IMAGE *loaded_image = NULL;                  /* image interface */
    EFI_GUID lipGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;      /* image interface GUID */

    /* get the loaded image protocol interface for our "image" */
    EFI_STATUS statusOf = uefi_call_wrapper(BS->HandleProtocol, 3, image, &lipGuid, (void **) &loaded_image);
    if (!loaded_image || statusOf != EFI_SUCCESS) {
      Print(L"Failed to get loaded image protocol\n");
      return NULL;
    }
    /* get the volume handle */
    return LibOpenRoot(loaded_image->DeviceHandle);
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
     * Read configuration
     * 
     * Get volume handle
     */
    EFI_FILE_HANDLE Volume = GetVolume(ImageHandle);
    
    if (Volume == NULL) {
      Print(L"Failed to get volume handle\n");
      CALL(EFI_ABORTED);
    }
    
    // Open file
    CHAR16              *FileName = L"EFI\\BOOT\\KEKBOOT.INI";
    EFI_FILE_HANDLE     FileHandle;
    
    CALL(uefi_call_wrapper(Volume->Open, 5, Volume, &FileHandle, FileName, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM));

    // Read file
    UINT64              ReadSize = FileSize(FileHandle);
    UINT8               *Buffer = AllocatePool(ReadSize);
    Print(L"Allocated buffer\n");

    uefi_call_wrapper(FileHandle->Read, 3, FileHandle, &ReadSize, Buffer);
    Print(L"Read file %s\n", FileName);
    
    // Close file
    CALL(uefi_call_wrapper(FileHandle->Close, 1, FileHandle));
    Print(L"Closed file %s\n", FileName);

    // Print contents of the read file
    UINT8 *CurrentPtr = Buffer;

    // Ensure the buffer is null-terminated for secure string printing
    Buffer[ReadSize] = '\0';
    
    while(*CurrentPtr != '\0') {
      Print(L"%c\n", *CurrentPtr);
      CurrentPtr++;
    }

    // Free the allocated buffer
    FreePool(Buffer);

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

