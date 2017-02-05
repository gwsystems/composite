#!/bin/sh

# output: list of exported functions by the interface (i.e. the stubs)

nm -g s_stub.o | awk '/_inv/{print $3}'
