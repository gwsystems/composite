# External Repositories

Composite current enables components to be defined externally from the main repo, thus providing increased autonomy in development.
Each repository in this directory is pulled from github, and can contain a set of components and composition scripts.
These repositories can be thought of as part of the Composite ecosystem, but not as part of the "standard library" of necessary functionalities.

A number of awkward realities with the current system:

- Composite does *not* maintain the repos downloaded here in any way.
    If the upstream repo is changed, you must manually do a `git pull` in the repo (or remove it so it is re-cloned).
	Comparably, if you update the external repo, you should manually commit/push, etc...
- The Composite repo ignores all repos in here.
    Never add them.
	The proper course of action would be to do a PR to the main Composite repo adding your component instead.

## TODO: Libraries and Interfaces

It should be possible to also define libraries and interfaces in external repositories.
This is more complicated as the dependencies for these are

1. not properly maintained by the composer (libraries aren't properly rebuilt when one of their dependencies changed), and
2. these dependencies are tracked by the build system, so updating them to enable dependencies are repositories requires most structural updates to the build system.

There are no hard blockers to doing this; we just need more time and will!
