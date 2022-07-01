## tests - pci

### Description

This component tests the PCI library implemented in `src/components/lib/pci`

### Usage and Assumptions
IMPORTANT: We are assuming QEMU version 1:2.11+dfsg-1ubuntu7.36 for these tests and hard-coding the expected values for the devices when running that version. 

If the tests are failing, check again that you are running with this QEMU version, or update `hw_profile.h` with the devices expected for your version.
