PIN_ROOT=/home/binary/pin/pin-3.6-97554-g31f0a167d-gcc-linux

CC=gcc-4.9
CXX=g++-4.9
CXXFLAGS=-Wall -Wno-unknown-pragmas -Wno-unused-variable -O2 -std=c++11 -fno-stack-protector -I$(TRITON_HOME)/src/libtriton/includes/ -L$(TRITON_HOME)/build/src/libtriton/

CURRENT_DIR=/media/sf_binary/picfi/

SPE_DIR=specimen/
SPE_TAR_DIR=specimen/bin/
SPE_TAR=spe_no_ssp

ARG=binary
FNO=-fno-stack-protector

.PHONY: all profiler return_address_violation symbol_table sumbol_table_test test_overwrite test clean

all: profiler return_address_violation

profiler: profiler/makefile profiler/makefile.rules profiler/profiler.cpp
	export PIN_ROOT=$(PIN_ROOT) && cd profiler && $(MAKE)

return_address_violation: specimen/return_address_violation.c
	gcc -Wall -g -o $(SPE_TAR_DIR)$(SPE_TAR) $(SPE_DIR)return_address_violation.c $(FNO)

symbol_table: symbol_table/symbol_table.cpp
	g++ -Wall -o symbol_table/bin/symbol_table symbol_table/symbol_table.cpp

symbol_table_test: symbol_table/bin/symbol_table
	./symbol_table/bin/symbol_table

test_overwrite: profiler/obj-intel64/profiler.so
	$(PIN_ROOT)/pin -t ./profiler/obj-intel64/profiler.so -- $(CURRENT_DIR)$(SPE_TAR_DIR)spe_no_ssp_overwrite $(ARG)

test: profiler/obj-intel64/profiler.so specimen/bin/spe_no_ssp
	$(PIN_ROOT)/pin -t ./profiler/obj-intel64/profiler.so -- $(CURRENT_DIR)$(SPE_TAR_DIR)$(SPE_TAR) $(ARG)

clean:
	cd profiler && rm -rf obj-intel64

