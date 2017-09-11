#!/bin/bash
find src -type f | egrep -v '(lib\/.*\/|linux|archives)' | egrep '\.[ch]$' |  xargs clang-format -i
