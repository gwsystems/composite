include Makefile.src
MAKEOPTIONS+= -I$(shell pwd)
export MAKEOPTIONS

# Order of which subdirectories are built first matters here.
# Interface files rely on library files, and component implementations
# rely on both the interfaces and the libs

all:
	$(MAKE) $(MAKEOPTIONS) -C lib
	$(MAKE) $(MAKEOPTIONS) -C interface
	$(MAKE) $(MAKEOPTIONS) -C implementation

clean:
	$(MAKE) $(MAKEOPTIONS) -C interface clean
	$(MAKE) $(MAKEOPTIONS) -C implementation clean
	$(MAKE) $(MAKEOPTIONS) -C lib clean

distclean:
	$(MAKE) $(MAKEOPTIONS) -C lib distclean

init:
	$(MAKE) $(MAKEOPTIONS) -C lib init
	$(MAKE) $(MAKEOPTIONS) -C interface init
	$(MAKE) $(MAKEOPTIONS) -C implementation init

component:
	$(MAKE) $(MAKEOPTIONS) -C implementation component

# Download a remote repository (e.g. from github). We're setting up a
# specific rule for the repo's path so that it won't redundantly clone
# the repo. Once it has been cloned, never clone it again.
REPO_ABSPATH = $(REPO_DIR)/$(REPO_PATH)
$(REPO_ABSPATH):
	$(GITCLONE) $(REPO_URL) $@

repo_external: $(REPO_ABSPATH)
