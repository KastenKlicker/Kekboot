# KekBoot
KekBoot decides the boot OS by checking the WakeUpType reported by the SMBIOS.

This works by chainloading the wanted OS.

### How to build
1. Clone the GNU EFI repository and follow the instructions the README.gnuefi.
2. Test if one of the GNU EFI apps is working
3. Clone this repository into the apps directory
4. Build the EFI application

### How to use
1. Create a UTF-16 file with 9 entries structured: partitionGUID=filePath partitionGUID=filePath...,
    where partitionGUID is the GUID of the partition of the chainload EFI file is placed and
    the path of the chainload EFI file. The position of the entry specify the WakeUpType.
    Both can be obtained with the `efibootmgr` command.
2. Create a new EFI variable like this:
    `efivar --write --name=1be4df61-93ca-11d2-aa0d-00e098032b8c-WakeUpType --datafile=myvar.data`
3. Check if the efi variable was created correctly:
    `efivar --print --name=1be4df61-93ca-11d2-aa0d-00e098032b8c-WakeUpType`
4. Rename the build EFI Application to BOOTX64.EFI and save it to the EFI partition
5. Reboot into the EFI partition
