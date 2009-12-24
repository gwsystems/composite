# Order matters here.  Interface files rely on library files, and
# component implementations rely on both the interfaces and the libs

all:
	make -C lib
	make -C interface
	make -C implementation

cp:
	make -C implementation cp

clean:
	make -C lib clean
	make -C interface clean
	make -C implementation clean

fresh:
	make -C lib fresh
	make -C interface fresh
	make -C implementation fresh
