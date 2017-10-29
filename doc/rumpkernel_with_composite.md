## Compiling the Rumpkernel
1. Ensure initialization steps [here](https://github.com/gwsystems/composite/blob/rumpkernel/doc/README.md) for Composite have been followed 
2. `$ cd src/`
3. Initialize submodules `$ make config`
4. Initialize the rumpkernel by running `$ make rk_init`
5. To compile the rumpkernel run `$ make rk`
6. To clean the rumpkernel run `$ make rk_clean`

## Running applications
1. `$ cd src/components/extern/rk/build/`
2. `$ ./runapps.sh <application>` 
    * `$ ./runapps.sh` with no argument will provide a list of available applications

### TODO
* how to add new applications
