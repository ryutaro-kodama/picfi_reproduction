PIN_HOME=/home/binary/triton/pin-2.14-71313-gcc.4.4.7-linux
TRITON_HOME=$(PIN_HOME)/source/tools/Triton

CC=gcc-4.9
CXX=g++-4.9
CXXFLAGS=-Wall -Wno-unknown-pragmas -Wno-unused-variable -O2 -std=c++11 -fno-stack-protector -I$(TRITON_HOME)/src/libtriton/includes/ -L$(TRITON_HOME)/build/src/libtriton/

BIN_DIR=bin/

.PHONY: all clean

all: loader.o triton_util.o disasm_util.o make_cfg branch cfg_testcode
loader.o: ../inc/loader.cc
	$(CXX) -std=c++11 -c ../inc/loader.cc && mv $@ $(BIN_DIR)

triton_util.o: triton_util.cc
	$(CXX) -std=c++11 -c triton_util.cc && mv $@ $(BIN_DIR)

disasm_util.o: disasm_util.cc
	$(CXX) -std=c++11 -c disasm_util.cc && mv $@ $(BIN_DIR)

make_cfg: make_cfg.cc
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)$@ $@.cc $(BIN_DIR)loader.o $(BIN_DIR)triton_util.o $(BIN_DIR)disasm_util.o -ltriton -lbfd -lcapstone

cfg_testcode: cfg_testcode.c
	$(CC) -g -o $(BIN_DIR)cfg_testcode cfg_testcode.c

test:
	./$(BIN_DIR)make_cfg $(BIN_DIR)cfg_testcode cfg_testcode.map 0x400608 0x400638 cfg_test.txt

clean:
	rm -f bin/*

