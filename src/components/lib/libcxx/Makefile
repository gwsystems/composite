BUILDDIR=./build
DIRS=$(BUILDDIR) $(BACKWARD_INCDIR) $(BITS_INCDIR) $(DEBUG_INCDIR) $(EXT_INCDIR)

.PHONY: all clean symlinks

all: symlinks libsupc++.a libstdc++.a 

include include.mk
$(DIRS):
	mkdir -p $@

symlinks: $(DIRS)
	find $(INCLUDE_INCDIR) -type l | xargs rm || true
	ln -s $(INCLUDE_SYMS_TARGET) $(INCLUDE_INCDIR)
	ln -s $(BACKWARD_SYMS_TARGET) $(BACKWARD_INCDIR)
	ln -s $(BITS_SYMS_TARGET) $(BITS_INCDIR)
	ln -s $(DEBUG_SYMS_TARGET) $(DEBUG_INCDIR)
	ln -s $(EXT_SYMS_TARGET) $(EXT_INCDIR)

clean:
	@make -C ./libstdc++-v3-4.8/libsupc++ clean
	@make -C ./libstdc++-v3-4.8 clean
	rm -f *.o *.a 
	rm -rf $(DIRS) 
	find $(INCLUDE_INCDIR) -type l | xargs rm || true

libsupc++.a:
	@make -C ./libstdc++-v3-4.8/libsupc++

libstdc++.a:
	@make -C ./libstdc++-v3-4.8
