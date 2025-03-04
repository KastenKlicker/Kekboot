# KekBoot
KekBoot decides the boot OS by checking the WakeUpType reported by the SMBIOS.

This works by setting the BootNext efivar and rebooting the system.

### How to use
1. Clone the GNU EFI repository and follow the instructions the README.gnuefi.
2. Test if one of the GNU EFI apps is working
3. Clone this repository into the apps directory
4. Build the EFI application
