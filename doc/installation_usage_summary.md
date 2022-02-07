Installation and Usage Summary
==============================

Example
-------

```bash
# $ARCH can be x86_64 or i386
# $SYS_ENV can be ubuntu-64 or fedora-64
# $YOUR_TOML_SCRIPT is the script to specify how to compsose your components, we have examples under the `composition_scripts` folder

1. git clone https://github.com/gwsystems/composite.git

2. cd composite
3. tools/init_env.sh $SYS_ENV # only need to do this step once when you download the system first time
4. ./cos init $ARCH
5. ./cos build
6. ./cos compose composition_scripts/$YOUR_TOML_SCRIPT $YOUR_OUTPUT_NAME
7. ./cos run $YOUR_OUTPUT_NAME
```

Recompile Steps
---------------

The following steps will need to be run every time you write some code and want to recompile and test. 

```bash
1. ./cos build
2. ./cos compose composition_scripts/$YOUR_TOML_SCRIPT $YOUR_OUTPUT_NAME
3. ./cos run $YOUR_OUTPUT_NAME
```
