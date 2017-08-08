.PHONY: all clean symlinks

all: libsupc++.a libstdc++.a 

clean:
	@make -C ./libstdc++-v3-4.8/libsupc++ clean
	@make -C ./libstdc++-v3-4.8 clean
	rm -f *.o *.a 

libsupc++.a:
	@make -C ./libstdc++-v3-4.8/libsupc++

libstdc++.a:
	@make -C ./libstdc++-v3-4.8
