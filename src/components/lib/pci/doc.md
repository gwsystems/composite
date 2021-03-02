## pci

This is the library for interacting with PCI devices.

### Description

This library provides functionality for PCI. See the test in `src/components/implementation/tests/pci` for examples of how this library might be used. 

Functions:
- `pci_dev_scan`
    - provide an array of `struct pci_dev` and the number of elements (`sz`) of that array
    - scans through the pci bus and fills the provided array
- `pci_dev_num`
    - iterates through the pci bus and returns the total number of devices on the bus
- `pci_dev_print`
    - iterates through the provided array and prints out device id, vendor id, and classcode for each device
- `pci_dev_get`
    - returns the device associated with the provided device id and vendor id, if one exists


### Usage and Assumptions

The test file assumes QEMU version 1:2.11+dfsg-1ubuntu7.36 and hard-codes the check for which devices we expect to be on the PCI bus.

TODO: Currently assuming PCI 1.0, in the future we might add support for PCIe
