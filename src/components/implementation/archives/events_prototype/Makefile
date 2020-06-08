# C_OBJS=cFE_entrypoint.o cFE_stub.o osapi.o osfiles.o osloader.o osqueue.o ostask.o ostimer.o osnetwork.o psp.o scheddev/sl.c scheddev/sl_mod_fprr.c
ASM_OBJS=
COMPONENT=rust_test.o
INTERFACES=
DEPENDENCIES=
ADDITIONAL_LIBS=-lcobj_format $(LIBSLRAW) -lsl_thd_static_backend -lsl_lock -lsl_mod_fprr
OBJLIBS += $(POSIX_LIB)

CRATE_DIR=events

XARGO_RUST_SRC=$(shell rustc --print sysroot)/lib/rustlib/src/rust/src/
RUSTFLAGS=-L $(shell pwd)/../../../lib/musl-1.1.11/lib/ -L $(shell pwd)/$(CRATE_DIR)/lib
RUST_TARGET_PATH=$(shell pwd)/$(CRATE_DIR)
export XARGO_RUST_SRC
export RUSTFLAGS
export RUST_TARGET_PATH

IF_LIB:=lib$(CRATE_DIR).a

pre: rust all

.PHONY: rust
rust:
	@cd $(CRATE_DIR) && . ~/.cargo/env && xargo build --target i686-unknown-composite-musl --release
	@cp $(CRATE_DIR)/target/i686-unknown-composite-musl/release/*.a .

include ../../Makefile.subsubdir

MANDITORY_LIB=simple_stklib.o
