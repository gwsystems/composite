CXX := g++
AR := ar
MV := mv
CXXFLAGS := -Wall -nostdinc -nostdlib -fno-stack-protector -fno-exceptions -std=gnu++11 -O3 $(INCLUDE) $(MUSLFLAG) -D MINIMAL_LIBCXX -m32
