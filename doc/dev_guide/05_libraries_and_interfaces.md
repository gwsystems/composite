# Libraries and Interfaces

Libraries are not very different from those in other systems, but as dependencies are explicit in Composite, their structure within the surrounding software is different.
Interfaces abstract the functional communication between components that form the core dependencies between components.

## Libraries in Composite

Libraries are initialized during `make init` and built during `make`.
Libraries in `src/components/lib/` are built in an undefined order, thus requiring the decoupling of `make init` which builds header files where necessary for specific libraries (for exampele, `ps` and `libc`), and `make` which allows each library to `#include` each other (thus requiring all headers to be present).

The build system guarantees the following:

- For a component, library, or interface, the dependencies for libraries are properly compiled and linked -- the corresponding include paths, library paths, and library objects are added into component compilation.
- For a library, its include paths and objects are made visible through the build system.
- All of these rules apply transitively.
	Libraries and interfaces required by libraries and interfaces, and so on.

## Building a Composite Library

## Creating a New Library

A script helps us make a new library:

``` sh
$ cd src/component/lib/
$ sh mklib.sh name
```

Creates a new library directory in `src/component/lib/name/`.
More details on libraries can be found in the corresponding chapter.

## Creating a New Interface

Same story for interfaces:

``` sh
$ cd src/component/interface/
$ sh mkinterface.sh name
```

Which will create a new interface in `src/component/interface/name/`.

## Integrating an External Library

### Considerations


If there are automatically generated header files, then those


## Debugging

*Thread Local Storage (TLS)*.

## FAQ
