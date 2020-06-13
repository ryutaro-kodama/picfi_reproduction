PIN_ROOT=/home/binary/pin/pin-3.6-97554-g31f0a167d-gcc-linux

CC=gcc-4.9
CXX=g++-4.9
CXXFLAGS=-Wall -Wno-unknown-pragmas -Wno-unused-variable -O2 -std=c++11 -fno-stack-protector -I$(TRITON_HOME)/src/libtriton/includes/ -L$(TRITON_HOME)/build/src/libtriton/

CURRENT_DIR=/media/sf_binary/picfi/

SPE_DIR=specimen/
SPE_TAR_DIR=specimen/bin/
SPE_TAR=spe_no_ssp

MAKE_CFG=triton/bin/make_cfg

ARG=binary
FNO=-fno-stack-protector
PAD72=123456789012345678901234567890123456789012345678901234567890123456789012

.PHONY: all profiler return_address_violation symbol_table sumbol_table_test test_overwrite test clean

all: profiler return_address_violation

profiler: profiler/makefile profiler/makefile.rules profiler/profiler.cpp
	export PIN_ROOT=$(PIN_ROOT) && cd profiler && $(MAKE)

return_address_violation: specimen/return_address_violation.c
	gcc -Wall -g -o $(SPE_TAR_DIR)$(SPE_TAR) $(SPE_DIR)return_address_violation.c $(FNO)

# test: profiler/obj-intel64/profiler.so specimen/bin/spe_no_ssp
# 	$(PIN_ROOT)/pin -t ./profiler/obj-intel64/profiler.so -- $(CURRENT_DIR)$(SPE_TAR_DIR)$(SPE_TAR) $(ARG)

##########################  for making symbol_table ############################
symbol_table: symbol_table/symbol_table.cpp
	g++ -Wall -o symbol_table/bin/symbol_table symbol_table/symbol_table.cpp

symbol_table_test: symbol_table/bin/symbol_table
	./symbol_table/bin/symbol_table
################################################################################

####################### for testing printf specimen #########################
#return_address_violation_printf: specimen/return_address_violation_printf.c
#	gcc -Wall -g -o $(SPE_TAR_DIR)$@ $(SPE_DIR)$@.c $(FNO)

cfg_printf: specimen/bin/return_address_violation_printf
	./$(MAKE_CFG) $^ map/printf.map 0x400598 0x4005db cfg/printf.txt

test_printf: profiler/obj-intel64/profiler.so
	$(PIN_ROOT)/pin -t ./profiler/obj-intel64/profiler.so \
	-- $(SPE_TAR_DIR)return_address_violation_printf $(ARG) cfg/printf.txt 400586 4005db

test_printf_violate: profiler/obj-intel64/profiler.so
	printf "$(PAD72)\xb0\x05\x40" \
	| $(PIN_ROOT)/pin -t ./profiler/obj-intel64/profiler.so \
	-- $(SPE_TAR_DIR)return_address_violation_printf $(ARG) cfg/printf.txt 400586 4005db
################################################################################

####################### for testing overwrite specimen #########################
# return_address_violation_overwrite: specimen/return_address_violation_overwrite.c
# 	gcc -Wall -g -o $(SPE_TAR_DIR)$@ $(SPE_DIR)$@.c $(FNO)

cfg_overwrite: specimen/bin/return_address_violation_overwrite
	./$(MAKE_CFG) $^ map/overwrite.map 0x4006e7 0x40073e cfg/overwrite.txt

test_overwrite_violate: profiler/obj-intel64/profiler.so
	$(PIN_ROOT)/pin -t ./profiler/obj-intel64/profiler.so \
	-- $(SPE_TAR_DIR)return_address_violation_overwrite $(ARG) cfg/overwrite.txt 4006a6 40073e
################################################################################

############ for testing function_address_violation specimen ###################
#function_address_violation: specimen/function_address_violation.c
#	gcc -Wall -g -o $(SPE_TAR_DIR)$@ $(SPE_DIR)$@.c $(FNO)

function_address_violation_compile: specimen/function_address_violation.c
	gcc -Wall -g -S $^ -o $(SPE_DIR)function_address_violation.s $(FNO)

function_address_violation_assemple: specimen/function_address_violation.s
	gcc -Wall -g $^ -o $(SPE_TAR_DIR)function_address_violation $(FNO)

#cfg_printf: specimen/bin/return_address_violation_printf
#	./$(MAKE_CFG) $^ map/printf.map 0x400598 0x4005db cfg/printf.txt

#test_printf: profiler/obj-intel64/profiler.so
#	$(PIN_ROOT)/pin -t ./profiler/obj-intel64/profiler.so \
	-- $(SPE_TAR_DIR)return_address_violation_printf $(ARG) cfg/printf.txt 400586 4005db

#test_printf_violate: profiler/obj-intel64/profiler.so
#	printf "$(PAD72)\xb0\x05\x40" \
	| $(PIN_ROOT)/pin -t ./profiler/obj-intel64/profiler.so \
	-- $(SPE_TAR_DIR)return_address_violation_printf $(ARG) cfg/printf.txt 400586 4005db
################################################################################

clean:
	cd profiler && rm -rf obj-intel64

