# Customize this for your setup

TOP_DIR=/home/gparmer/research/composite/src/

TRANS_DIR=/home/gparmer/transfer
DEST_DIR=/root/experiments

KDIR=$(TOP_DIR)/kernel/
KINC=$(TOP_DIR)/kernel/include/
SHAREDINC=$(TOP_DIR)/kernel/include/shared/
CDIR=$(TOP_DIR)/components/
LDIR=/home/gparmer/research/linux-2.6.22
TEST_DIR=/root/experiments/

LINKER_DIR=$(TOP_DIR)/platform/linux/link_load/
NET_DIR=$(TOP_DIR)/platform/linux/net/

MAKE=make
CC=gcc
AR=ar
LD=ld
AS=$(CC)

