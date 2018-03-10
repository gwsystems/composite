import subprocess
import os

# Start in the right directory
os.chdir("./tools")

# Generate a .img that is half a meg in size, populate with 0's
subprocess.call(["dd", \
        "if=/dev/zero", \
        "iflag=fullblock", \
        "of=data.iso", \
        "bs=512", \
        "count=1024"])

# Execute mkfs.ext2 on .img
p = subprocess.Popen("mkfs.ext2 data.iso", shell=True, stdout=subprocess.PIPE ,stderr=subprocess.PIPE, stdin=subprocess.PIPE)
p.communicate(input="y")

# objdump the new ext2 image into a .o and move it to the right directory
subprocess.call(["objcopy", "-I", "binary", "-O", "elf32-i386", "-B", "i386", "data.iso", "backing.o"])
subprocess.call(["mkdir", "../rumprun/lib/libbmk_rumpuser/cos/"])
subprocess.call(["cp", "backing.o", "../rumprun/lib/libbmk_rumpuser/cos/"])
