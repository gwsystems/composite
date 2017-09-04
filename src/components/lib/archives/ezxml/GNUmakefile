# Makefile
#
# Copyright 2005 Aaron Voisine <aaron@voisine.org>
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

CC = gcc
AR = ar
RM = rm -f
CFLAGS = -Wall -O2
DEBUG_CFLAGS = -O0 -g
OBJS = ezxml.o
LIB = libezxml.a
TEST = ezxmltest
ifdef NOMMAP
CFLAGS += -D EZXML_NOMMAP
endif
ifdef DEBUG
CFLAGS += $(DEBUG_CFLAGS)
endif

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $(LIB) $(OBJS)

nommap: CFLAGS += -D EZXML_NOMMAP
nommap: all

debug: CFLAGS += $(DEBUG_CFLAGS)
debug: all

test: CFLAGS += $(DEBUG_CFLAGS)
test: $(TEST)

$(TEST): CFLAGS += -D EZXML_TEST
$(TEST): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

ezxml.o: ezxml.h ezxml.c

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) $(OBJS) $(LIB) $(TEST) *~